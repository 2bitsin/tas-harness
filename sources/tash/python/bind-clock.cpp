#include "tash/python/bindings.hpp"

#include <cstdint>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::ScenarioRun;

  auto BindClock(py::module_& module) -> void
  {
    py::class_<ScenarioRun> run{ RunClass(module) };

    run.def("pace",
            [](ScenarioRun& self, double rate) { self.Live().Pace(rate); },
            py::arg("rate"));

    run.def("frames",
            [](ScenarioRun& self) { return self.Live().Frames(); });

    run.def("harness_time",
            [](ScenarioRun& self) { return self.Live().HarnessSeconds(); });

    run.def("fps", [](ScenarioRun& self) { return self.Live().Fps(); });
  }
}
