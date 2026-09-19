#pragma once
// The watches one run follows, in the order a trace indexes them by.

#include "tash/utilities/outcome.hpp"
#include "tash/watches/watch-spec.hpp"
#include "tash/watches/watch.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace tash::watches::detail::watch_set
{
  using utilities::Result;
  using watch::Watch;
  using watch_spec::WatchSpec;

  class WatchSet
  {
  public:
    WatchSet() = default;
    explicit WatchSet(std::vector<Watch> watches);

    // Refuses a duplicate name: a predicate asks for a watch by name and
    // two answers to one question is not an answer.
    [[nodiscard]] static auto From(std::vector<WatchSpec> const& specs)
      -> Result<WatchSet>;

    [[nodiscard]] auto Count() const noexcept -> std::size_t;
    [[nodiscard]] auto Empty() const noexcept -> bool;
    [[nodiscard]] auto All() const noexcept -> std::vector<Watch> const&;
    [[nodiscard]] auto At(std::size_t index) const -> Result<Watch>;
    [[nodiscard]] auto IndexOf(std::string_view name) const
      -> Result<std::size_t>;

  private:
    std::vector<Watch> _watches;
  };
}

namespace tash::watches
{
  using detail::watch_set::WatchSet;
}
