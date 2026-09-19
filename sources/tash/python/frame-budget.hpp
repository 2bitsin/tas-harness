#pragma once
// The frames one python job may run. It counts on the run's own frame
// counter, which never goes backwards, so every path that steps is counted.

#include "tash/utilities/outcome.hpp"

#include <atomic>
#include <cstdint>

namespace tash::python::detail::frame_budget
{
  using utilities::Outcome;

  inline constexpr std::uint64_t UNLIMITED{ 0 };

  class FrameBudget
  {
  public:
    FrameBudget()                                    = default;
    FrameBudget(FrameBudget const&)                  = delete;
    auto operator = (FrameBudget const&) -> FrameBudget& = delete;

    // Counts from the frames the run has made now.
    auto Begin(std::uint64_t made, std::uint64_t limit) noexcept -> void;

    auto End() noexcept -> void;

    // Stops the job from the thread that is not running it.
    auto Stop() noexcept -> void;

    [[nodiscard]] auto Limit() const noexcept -> std::uint64_t
    { return _limit; }

    [[nodiscard]] auto Spent(std::uint64_t made) const noexcept
      -> std::uint64_t;

    // Refuses a call that may run `wanted` frames past what is left.
    [[nodiscard]] auto Room(std::uint64_t made,
                            std::uint64_t wanted) const -> Outcome;

  private:
    std::uint64_t     _from{ 0 };
    std::uint64_t     _limit{ UNLIMITED };
    std::atomic<bool> _stopped{ false };
  };
}

namespace tash::python
{
  using detail::frame_budget::FrameBudget;
  using detail::frame_budget::UNLIMITED;
}
