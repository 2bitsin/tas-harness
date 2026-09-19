#include "tash/python/bindings.hpp"

#include "tash/perception/region.hpp"

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <filesystem>
#include <optional>
#include <string>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::ScenarioRun;

  namespace
  {
    auto Cropped(std::optional<std::string> const& region)
      -> std::optional<perception::Region>
    {
      if (!region || region->empty())
        return { };
      return Given(perception::RegionFrom(*region));
    }
  }

  auto BindShots(py::module_& module) -> void
  {
    py::class_<ScenarioRun> run{ RunClass(module) };

    run.def("look",
            [](ScenarioRun& self,
               std::optional<std::filesystem::path> const& path,
               std::optional<std::string> const& region)
            { return Given(self.Look(path, Cropped(region))); },
            py::arg("path") = std::optional<std::filesystem::path>{ },
            py::arg("region") = std::optional<std::string>{ });

    run.def("shot",
            [](ScenarioRun& self, std::optional<std::string> const& region)
            { return Given(self.Look(std::nullopt, Cropped(region))); },
            py::arg("region") = std::optional<std::string>{ });

    run.def("bundle", [](ScenarioRun& self) { return self.BundleRoot(); });

    run.def("report",
            [](ScenarioRun& self) { return Given(self.Report()); });
  }
}
