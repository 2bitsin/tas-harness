#include "tash/python/bindings.hpp"

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <cstdint>
#include <filesystem>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::ScenarioRun;
  using tape_sink::TapeWritten;

  namespace
  {
    // A tape runs as many frames as it takes, so a play asks for the one
    // frame it cannot avoid running and is refused when that is too many.
    constexpr std::uint64_t ONE_FRAME{ 1 };
  }

  auto BindTape(py::module_& module) -> void
  {
    py::class_<ScenarioRun> run{ RunClass(module) };

    run.def("play",
            [](ScenarioRun& self, std::filesystem::path const& file)
            {
              Allowed(self, ONE_FRAME);
              tape::PlayCounts const counts{ Given(self.Play(file)) };
              py::dict played;
              played["segments"] = counts.segments;
              played["frames"] = counts.frames;
              played["waited"] = counts.waited;
              played["transitions"] = counts.transitions;
              played["retries"] = counts.retries;
              return played;
            },
            py::arg("tape"));

    run.def("tape",
            [](ScenarioRun& self)
            {
              TapeWritten const written{ Given(self.Tape()) };
              py::dict taped;
              taped["file"] = written.file;
              taped["frames"] = written.frames;
              return taped;
            });

    run.def("line",
            [](ScenarioRun& self) { return Given(self.LineFrames()); });
  }
}
