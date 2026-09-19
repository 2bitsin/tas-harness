#include "tash/python/bindings.hpp"

#include <pybind11/stl.h>

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::ScenarioRun;

  namespace
  {
    // An anchor predicate in the mcp tools' text form, or the caller's own
    // answer, which is what expect has always taken.
    using Condition = std::variant<bool, std::string>;

    // A failed expectation is the scenario's own assertion, so it reads as
    // one in the traceback the cli hands back.
    [[noreturn]] auto Fail(std::string const& message) -> void
    {
      PyErr_SetString(PyExc_AssertionError, message.c_str());
      throw py::error_already_set{};
    }

    auto Expect(ScenarioRun& self, std::string const& name,
                Condition const& condition, std::string text, bool raises)
      -> void
    {
      bool held{ false };
      if (std::holds_alternative<bool>(condition))
        held = std::get<bool>(condition);
      else
      {
        tape::Judged const seen{
          Given(self.Judged(std::get<std::string>(condition))) };
        held = seen.passed;
        if (text.empty())
          text = seen.wording;
      }
      Done(self.Judge(name, held, text));
      if (!held && raises)
        Fail(std::format("{}: {}", name,
                         text.empty() ? "did not hold" : text));
    }
  }

  auto BindTrace(py::module_& module) -> void
  {
    py::class_<ScenarioRun> run{ RunClass(module) };

    run.def("mark",
            [](ScenarioRun& self, std::string name,
               std::optional<std::string> group)
            { Done(self.Mark(std::move(name),
                             std::move(group).value_or(std::string{ }))); },
            py::arg("name"), py::arg("group") = py::none());

    run.def("clip",
            [](ScenarioRun& self, std::string label,
               std::optional<std::string> const& from_mark,
               std::optional<std::int64_t> from_frame)
            {
              std::uint64_t const from{
                Given(self.ClipStart(from_mark, from_frame)) };
              Done(self.Clip(std::move(label), from));
            },
            py::arg("label"), py::arg("from_mark") = py::none(),
            py::arg("from_frame") = py::none());

    run.def("judge",
            [](ScenarioRun& self, std::string name, bool passed,
               std::string text)
            { Done(self.Judge(std::move(name), passed, std::move(text))); },
            py::arg("name"), py::arg("passed"), py::arg("text") = "");

    run.def("expect", &Expect, py::arg("name"), py::arg("condition"),
            py::arg("text") = "", py::arg("raises") = true);
  }
}
