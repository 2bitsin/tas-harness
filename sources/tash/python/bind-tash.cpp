#include "tash/python/bindings.hpp"

#include <cstdint>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  namespace
  {
    constexpr char const* MODULE_DOC{
      "The harness a tash scenario drives: `run` is the session the cli "
      "opened, and the module functions read its latest frame." };

    constexpr char const* BUDGET_EXCEPTION{ "BudgetExceeded" };
  }

  auto RunClass(py::module_& module) -> py::class_<scenario_run::ScenarioRun>
  {
    return py::reinterpret_borrow<py::class_<scenario_run::ScenarioRun>>(
      module.attr(RUN_TYPE));
  }

  auto RunOf(py::handle module) -> scenario_run::ScenarioRun&
  {
    py::object const held{ module.attr(RUN_NAME) };
    if (held.is_none())
      throw std::runtime_error{
        "python: there is no run; a scenario is started by "
        "`tash run --scenario`" };
    return held.cast<scenario_run::ScenarioRun&>();
  }

  auto Allowed(scenario_run::ScenarioRun const& run, std::uint64_t frames)
    -> void
  {
    if (utilities::Outcome const room{ run.Allowed(frames) }; !room)
      throw BudgetExceeded{ room.error() };
  }

  auto BindTash(py::module_& module) -> void
  {
    module.doc() = MODULE_DOC;
    py::register_exception<BudgetExceeded>(module, BUDGET_EXCEPTION);
    py::class_<scenario_run::ScenarioRun>{ module, RUN_TYPE };
    module.attr(RUN_NAME) = py::none();

    BindSession(module);
    BindClock(module);
    BindTape(module);
    BindPerception(module);
    BindTrace(module);
    BindShots(module);
    BindWatches(module);
    BindSearch(module);
    BindPlan(module);
    BindHunt(module);
  }
}
