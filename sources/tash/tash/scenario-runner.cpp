#include "tash/tash/scenario-runner.hpp"

#include "tash/python/bindings.hpp"

#include <pybind11/eval.h>

#include <utility>

#include <format>
#include <string>

PYBIND11_EMBEDDED_MODULE(tash, module)
{
  tash::python::BindTash(module);
}

namespace tash::cli::detail::scenario_runner
{
  namespace py = pybind11;

  using utilities::Refused;

  namespace
  {
    constexpr char const* MODULE_NAME{ "tash" };
    constexpr char const* MAIN_NAME{ "__main__" };
    constexpr char const* REDIRECT{ "contextlib" };
    constexpr char const* REDIRECT_OUT{ "redirect_stdout" };
    constexpr char const* REDIRECT_ERR{ "redirect_stderr" };
    constexpr char const* SINK_NAME{ "_TashPrinting" };

    // A text stream of its own, because a StringIO only gives its text back
    // at the end and a detached job is read while it still runs.
    constexpr char const* SINK_SOURCE{
      "import io\n"
      "class _TashPrinting(io.TextIOBase):\n"
      "    def __init__(self, put):\n"
      "        self._put = put\n"
      "    def writable(self):\n"
      "        return True\n"
      "    def write(self, text):\n"
      "        self._put(text)\n"
      "        return len(text)\n" };

    [[nodiscard]] auto SinkFor(Printed const& onto) -> py::object
    {
      py::dict scope;
      py::exec(SINK_SOURCE, scope);
      return scope[SINK_NAME](py::cpp_function(
        [onto](std::string const& text) { onto(text); }));
    }

    auto TracebackOf(py::error_already_set& raised) -> std::string
    {
      try
      {
        py::module_ const traceback{ py::module_::import("traceback") };
        py::list const lines{
          traceback.attr("format_exception")(raised.value()) };
        std::string text;
        for (py::handle const line : lines)
          text += line.cast<std::string>();
        return text;
      }
      catch (py::error_already_set const&)
      {
        return raised.what();
      }
    }
  }

  auto ScenarioRunner::Play(python::ScenarioRun& run,
                            std::filesystem::path const& file) -> Outcome
  {
    if (!std::filesystem::exists(file))
      return Refused("python: no scenario at {}", file.string());

    py::module_ module{ py::module_::import(MODULE_NAME) };
    module.attr(python::RUN_NAME)
      = py::cast(&run, py::return_value_policy::reference);

    py::dict globals;
    globals["__name__"] = MAIN_NAME;
    globals["__file__"] = file.string();

    std::string refusal;
    try
    {
      py::eval_file(file.string(), globals);
    }
    catch (py::error_already_set& raised)
    {
      refusal = std::format("python: {} raised\n{}", file.string(),
                            TracebackOf(raised));
    }
    module.attr(python::RUN_NAME) = py::none();
    if (!refusal.empty())
      return std::unexpected{ refusal };
    return {};
  }

  auto ScenarioRunner::Bind(python::ScenarioRun& run) -> void
  {
    py::module_ module{ py::module_::import(MODULE_NAME) };
    module.attr(python::RUN_NAME)
      = py::cast(&run, py::return_value_policy::reference);
    _globals["__name__"] = MAIN_NAME;
    _globals[MODULE_NAME] = module;
  }

  auto ScenarioRunner::Unbind() -> void
  {
    py::module_ module{ py::module_::import(MODULE_NAME) };
    module.attr(python::RUN_NAME) = py::none();
  }

  auto ScenarioRunner::Evaluate(std::string const& source, Printed onto,
                                Printed errors) -> Result<std::string>
  {
    py::object redirecting;
    py::object redirecting_errors;
    try
    {
      py::module_ const contextlib{ py::module_::import(REDIRECT) };
      redirecting = contextlib.attr(REDIRECT_OUT)(SinkFor(onto));
      redirecting.attr("__enter__")();
      redirecting_errors
        = contextlib.attr(REDIRECT_ERR)(SinkFor(errors));
      redirecting_errors.attr("__enter__")();
    }
    catch (py::error_already_set& raised)
    {
      return Refused("python: {}", TracebackOf(raised));
    }

    std::string answered;
    std::string refusal;
    try
    {
      // An expression answers with its value; anything else answers with
      // whatever it printed.
      py::object const value{ py::eval<py::eval_expr>(source, _globals) };
      if (!value.is_none())
        answered = py::str(value).cast<std::string>();
    }
    catch (py::error_already_set& raised)
    {
      if (raised.matches(PyExc_SyntaxError))
        try
        {
          py::exec(source, _globals);
        }
        catch (py::error_already_set& again)
        {
          refusal = TracebackOf(again);
        }
      else
        refusal = TracebackOf(raised);
    }

    try
    {
      redirecting_errors.attr("__exit__")(py::none(), py::none(), py::none());
      redirecting.attr("__exit__")(py::none(), py::none(), py::none());
    }
    catch (py::error_already_set& raised)
    {
      if (refusal.empty())
        refusal = TracebackOf(raised);
    }

    if (!refusal.empty())
      return Refused("python: {}", refusal);
    return answered;
  }
}
