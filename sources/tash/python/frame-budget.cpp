#include "tash/python/frame-budget.hpp"

#include <algorithm>

namespace tash::python::detail::frame_budget
{
  using utilities::Refused;

  auto FrameBudget::Begin(std::uint64_t made, std::uint64_t limit) noexcept
    -> void
  {
    _from = made;
    _limit = limit;
    _stopped.store(false, std::memory_order_relaxed);
  }

  auto FrameBudget::End() noexcept -> void
  {
    _limit = UNLIMITED;
    _stopped.store(false, std::memory_order_relaxed);
  }

  auto FrameBudget::Stop() noexcept -> void
  {
    _stopped.store(true, std::memory_order_relaxed);
  }

  auto FrameBudget::Spent(std::uint64_t made) const noexcept -> std::uint64_t
  {
    return made - std::min(made, _from);
  }

  auto FrameBudget::Room(std::uint64_t made, std::uint64_t wanted) const
    -> Outcome
  {
    std::uint64_t const spent{ Spent(made) };
    if (_stopped.load(std::memory_order_relaxed))
      return Refused("python: the job was cancelled after {} frames", spent);
    if (_limit == UNLIMITED)
      return { };
    if (wanted <= _limit - std::min(_limit, spent))
      return { };
    return Refused("python: frame budget: {} of {} frames run, {} left, and "
                   "this call may run {}; catch tash.BudgetExceeded, or pass "
                   "a bigger frame_budget", spent, _limit,
                   _limit - std::min(_limit, spent), wanted);
  }
}
