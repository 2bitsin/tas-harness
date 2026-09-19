#include "tash/python/bindings.hpp"

#include "tash/python/memory-hunt.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/number-format.hpp"

#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using memory_hunt::MemoryHunt;
  using scenario_run::ScenarioRun;

  namespace
  {
    constexpr char const* HUNT_TYPE{ "Hunt" };

    auto Listed(MemoryHunt const& hunt, std::size_t limit) -> py::list
    {
      py::list survivors;
      for (watches::Candidate const& candidate : hunt.Candidates(limit))
        survivors.append(py::make_tuple(candidate.address, candidate.value));
      return survivors;
    }

    auto Worded(MemoryHunt const& hunt) -> std::string
    {
      return std::format("Hunt(region={}, width={}, endian={}, stride={}, "
                         "candidates={})",
                         hunt.Region(), hunt.Format().width,
                         watches::NameOf(hunt.Format().endianness),
                         hunt.Stride(), hunt.Count());
    }

    auto Over(ScenarioRun& run, std::string region, std::uint32_t width,
              std::string const& endian, std::uint32_t stride) -> MemoryHunt
    {
      watches::Endianness const endianness{
        Given(watches::EndiannessOf(endian)) };
      return Given(MemoryHunt::Of(
        watches::MemoryMap::Of(run.Live()), std::move(region),
        watches::NumberFormat{ width, endianness, false }, stride));
    }
  }

  auto BindHunt(py::module_& module) -> void
  {
    py::class_<MemoryHunt>{ module, HUNT_TYPE }
      .def("step",
           [](MemoryHunt& self, std::string const& how,
              std::optional<std::int64_t> value)
           { return Given(self.Step(how, value)); },
           py::arg("how"), py::arg("value") = py::none())
      .def("candidates", &Listed, py::arg("limit") = CANDIDATE_LIMIT)
      .def("reset", &MemoryHunt::Reset)
      .def("__len__", &MemoryHunt::Count)
      .def("__repr__", &Worded);

    RunClass(module).def("hunt", &Over, py::arg("region"),
                         py::arg("width") = watches::WIDTH_WORD,
                         py::arg("endian") = "little", py::arg("stride") = 0u);
  }
}
