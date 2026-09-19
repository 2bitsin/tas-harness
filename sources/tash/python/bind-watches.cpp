#include "tash/python/bindings.hpp"

#include "tash/watches/number-format.hpp"
#include "tash/watches/watch.hpp"

#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::ScenarioRun;

  namespace
  {
    // Latin-1, not utf-8: every byte is one code point, so no byte of a
    // game's own glyph table is lost or refused, and the ascii a status
    // line is made of reads back unchanged.
    auto Latin1(std::string const& read) -> py::str
    {
      PyObject* const text{ PyUnicode_DecodeLatin1(
        read.data(), static_cast<Py_ssize_t>(read.size()), nullptr) };
      if (text == nullptr)
        throw py::error_already_set{ };
      return py::reinterpret_steal<py::str>(text);
    }
  }

  // The watches module fills a run's WatchValues in; until a run carries
  // one, every name answers None and the table is empty.
  auto BindWatches(py::module_& module) -> void
  {
    py::class_<ScenarioRun> run{ RunClass(module) };

    run.def("watch",
            [](ScenarioRun& self, std::string const& name)
              -> std::optional<std::int64_t>
            {
              WatchValues const* const values{ self.Watches() };
              return values == nullptr ? std::nullopt : values->Value(name);
            },
            py::arg("name"));

    run.def("memory",
            [](ScenarioRun& self, std::string const& region,
               std::uint32_t address, std::size_t count)
            {
              std::vector<std::byte> const block{
                Given(self.Memory(region, address, count)) };
              return py::bytes(reinterpret_cast<char const*>(block.data()),
                               block.size());
            },
            py::arg("region"), py::arg("address"), py::arg("count"));

    run.def("string",
            [](ScenarioRun& self, std::string const& region,
               std::uint32_t address, std::size_t length)
            { return Latin1(Given(self.String(region, address, length))); },
            py::arg("region"), py::arg("address"), py::arg("length"));

    run.def("word",
            [](ScenarioRun& self, std::string const& region,
               std::uint32_t address)
            {
              return Given(self.Number(region, address,
                                       watches::WIDTH_WORD));
            },
            py::arg("region"), py::arg("address"));

    run.def("long",
            [](ScenarioRun& self, std::string const& region,
               std::uint32_t address)
            {
              return Given(self.Number(region, address,
                                       watches::WIDTH_LONG));
            },
            py::arg("region"), py::arg("address"));

    run.def("memory_stable",
            [](ScenarioRun& self, std::string const& region,
               std::uint32_t address, std::size_t count, std::uint64_t frames,
               std::uint64_t timeout_frames)
            {
              Allowed(self, timeout_frames);
              return Given(self.MemoryStable(region, address, count, frames,
                                             timeout_frames));
            },
            py::arg("region"), py::arg("address"), py::arg("count"),
            py::arg("frames"),
            py::arg("timeout_frames")
              = scenario_run::STABLE_TIMEOUT_FRAMES);

    module.def("watch",
               [module](std::string const& name) -> std::optional<std::int64_t>
               {
                 WatchValues const* const values{ RunOf(module).Watches() };
                 return values == nullptr ? std::nullopt : values->Value(name);
               },
               py::arg("name"));

    module.def("watches",
               [module]
               {
                 py::dict table;
                 WatchValues const* const values{ RunOf(module).Watches() };
                 if (values == nullptr)
                   return table;
                 for (std::string const& name : values->Names())
                   table[py::str(name)] = py::cast(values->Value(name));
                 return table;
               });
  }
}
