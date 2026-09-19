#pragma once
// What a moving call raises once the job's frame budget is spent, bound as
// `tash.BudgetExceeded` so a scenario can catch its own runaway loop.

#include <stdexcept>

namespace tash::python::detail::budget_exceeded
{
  class BudgetExceeded : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };
}

namespace tash::python
{
  using detail::budget_exceeded::BudgetExceeded;
}
