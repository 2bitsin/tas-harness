#pragma once
// One Bind per area of the scenario API, assembled by BindTash: the same
// set of sources back both the extension module and the embedded one.

#include "tash/python/budget-exceeded.hpp"
#include "tash/python/scenario-run.hpp"
#include "tash/utilities/outcome.hpp"

#include <pybind11/pybind11.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace tash::python::detail::bindings
{
  inline constexpr char const* RUN_TYPE{ "Run" };
  inline constexpr char const* RUN_NAME{ "run" };

  auto BindTash(pybind11::module_& module) -> void;

  auto BindSession(pybind11::module_& module) -> void;
  auto BindClock(pybind11::module_& module) -> void;
  auto BindTape(pybind11::module_& module) -> void;
  auto BindPerception(pybind11::module_& module) -> void;
  auto BindTrace(pybind11::module_& module) -> void;
  auto BindShots(pybind11::module_& module) -> void;
  auto BindWatches(pybind11::module_& module) -> void;
  auto BindSearch(pybind11::module_& module) -> void;
  auto BindPlan(pybind11::module_& module) -> void;
  auto BindHunt(pybind11::module_& module) -> void;

  // The class every area hangs its methods on; BindTash registers it once.
  [[nodiscard]] auto RunClass(pybind11::module_& module)
    -> pybind11::class_<scenario_run::ScenarioRun>;

  // `tash.run`, which the cli sets before the scenario starts.
  [[nodiscard]] auto RunOf(pybind11::handle module)
    -> scenario_run::ScenarioRun&;

  // Raises BudgetExceeded when a call that may run that many frames has
  // no budget left; every moving call asks before it runs one.
  auto Allowed(scenario_run::ScenarioRun const& run, std::uint64_t frames)
    -> void;

  // A refusal is an exception here: a scenario is a script, and a script
  // that ignores a failed call would go on against a session that moved.
  template <typename Held>
  auto Given(utilities::Result<Held> result) -> Held
  {
    if (!result)
      throw std::runtime_error{ result.error() };
    return std::move(*result);
  }

  inline auto Done(utilities::Outcome const& outcome) -> void
  {
    if (!outcome)
      throw std::runtime_error{ outcome.error() };
  }
}

namespace tash::python
{
  using detail::bindings::BindTash;
  using detail::bindings::RUN_NAME;
}
