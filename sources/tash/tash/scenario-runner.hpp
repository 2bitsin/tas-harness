#pragma once
// The embedded interpreter a scenario runs in: one per process, answering a
// raised exception with the traceback a reader needs.

#include "tash/python/scenario-run.hpp"
#include "tash/utilities/outcome.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace tash::cli::detail::scenario_runner
{
  using utilities::Outcome;
  using utilities::Result;

  // Where the printing goes as it happens, for a caller that cannot wait.
  using Printed = std::function<void(std::string_view)>;

  class ScenarioRunner
  {
  public:
    ScenarioRunner()  = default;
    ~ScenarioRunner() = default;

    ScenarioRunner(ScenarioRunner const&)                    = delete;
    auto operator = (ScenarioRunner const&) -> ScenarioRunner& = delete;

    [[nodiscard]] auto Play(python::ScenarioRun& run,
                            std::filesystem::path const& file) -> Outcome;

    // Bound until unbound: what one Evaluate defines, the next one has.
    auto Bind(python::ScenarioRun& run) -> void;
    auto Unbind() -> void;

    // Both streams go to their sink as written; the answer is the value.
    [[nodiscard]] auto Evaluate(std::string const& source, Printed onto,
                                Printed errors) -> Result<std::string>;

  private:
    // Declared after the interpreter, so it is gone before finalisation.
    pybind11::scoped_interpreter _interpreter{ };
    pybind11::dict               _globals{ };
  };
}

namespace tash::cli
{
  using detail::scenario_runner::Printed;
  using detail::scenario_runner::ScenarioRunner;
}
