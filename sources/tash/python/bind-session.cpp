#include "tash/python/bindings.hpp"

#include "tash/tape/anchor.hpp"
#include "tash/tape/channel.hpp"

#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::CheckpointKind;
  using scenario_run::Observation;
  using scenario_run::ScenarioRun;

  namespace
  {
    // `p1.start`, a button name, or a list of either.
    using Buttons = std::variant<std::string, std::vector<std::string>>;

    // An anchor predicate in the mcp tools' text form, or the callable the
    // scenario API has always taken.
    using Until = std::variant<std::string, std::function<bool()>>;

    auto NamesOf(Buttons const& buttons) -> std::vector<std::string>
    {
      if (std::holds_alternative<std::string>(buttons))
        return { std::get<std::string>(buttons) };
      return std::get<std::vector<std::string>>(buttons);
    }

    auto PadsOf(Observation const& seen) -> py::dict
    {
      py::dict pads;
      for (std::size_t port{ 0 }; port < session::PORTS; ++port)
        pads[py::str(std::format("{}{}", tape::PORT_PREFIX, port + 1))]
          = py::cast(seen.pads[port]);
      return pads;
    }

    auto Observed(py::handle module) -> py::dict
    {
      Observation const seen{ Given(RunOf(module).Observe()) };

      py::dict answered;
      answered["frame"] = seen.frame;
      answered["time"] = seen.time;
      answered["width"] = seen.width;
      answered["height"] = seen.height;
      answered["exact"] = tape::HashText(seen.exact);
      answered["difference"] = tape::HashText(seen.difference);
      answered["perceptual"] = tape::HashText(seen.perceptual);
      answered["change"] = seen.change;
      answered["pad"] = PadsOf(seen);
      answered["watches"] = module.attr("watches")();
      return answered;
    }
  }

  auto BindSession(py::module_& module) -> void
  {
    py::class_<ScenarioRun> run{ RunClass(module) };

    run.def("step",
            [](ScenarioRun& self, std::uint64_t frames)
            { Allowed(self, frames); self.Live().Step(frames); },
            py::arg("frames") = 1);

    run.def("run_until",
            [](ScenarioRun& self, Until const& until,
               std::uint64_t timeout_frames)
            {
              Allowed(self, timeout_frames);
              if (std::holds_alternative<std::string>(until))
                return Given(self.RunUntil(std::get<std::string>(until),
                                           timeout_frames));
              return Given(self.Live().RunUntil(
                std::get<std::function<bool()>>(until), timeout_frames));
            },
            py::arg("predicate"), py::arg("timeout_frames"));

    run.def("anchor",
            [](ScenarioRun& self, std::string const& predicate)
            { return Given(self.Judged(predicate)).passed; },
            py::arg("predicate"));

    run.def("hold",
            [](ScenarioRun& self, std::size_t port, Buttons const& buttons)
            { Done(self.Hold(port, NamesOf(buttons))); },
            py::arg("port"), py::arg("buttons"));

    run.def("release",
            [](ScenarioRun& self, std::size_t port, Buttons const& buttons)
            { Done(self.Release(port, NamesOf(buttons))); },
            py::arg("port"),
            py::arg("buttons") = Buttons{ std::vector<std::string>{} });

    run.def("pad",
            [](ScenarioRun& self, std::size_t port)
            { return Given(self.Held(port)); },
            py::arg("port"));

    run.def("checkpoint",
            [](ScenarioRun& self, std::string name, bool scratch)
            {
              static_cast<void>(Given(self.Checkpoint(
                std::move(name), scratch ? CheckpointKind::SCRATCH
                                         : CheckpointKind::NAMED)));
            },
            py::arg("name"), py::arg("scratch") = false);

    run.def("restore",
            [](ScenarioRun& self, std::string const& name)
            { static_cast<void>(Given(self.Restore(name))); },
            py::arg("name"));

    run.def("forget",
            [](ScenarioRun& self, std::string const& name)
            { self.Forget(name); },
            py::arg("name"));

    run.def("reset", [](ScenarioRun& self) { Done(self.Reset()); });

    run.def("observe", [module](ScenarioRun&) { return Observed(module); });
  }
}
