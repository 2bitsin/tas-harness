#pragma once
// The search the scenario API exposes as `tash.search`: the emulator is the
// model, so a candidate is judged by applying it to a restored state.

#include "tash/python/scenario-run.hpp"

#include <pybind11/pybind11.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace tash::python::detail::state_search
{
  // The search's own checkpoints; the slash keeps them clear of a scenario's.
  inline constexpr std::string_view CHECKPOINT_PREFIX{ "tash.search/" };

  struct SearchResult
  {
    pybind11::object best{ pybind11::none() };
    pybind11::object score{ pybind11::none() };
    std::uint64_t    trials{ 0 };
    std::uint64_t    frames{ 0 };
    double           seconds{ 0.0 };
  };

  class StateSearch
  {
  public:
    // `restored` is called with the run after every restore to the seed,
    // the last one included; none is pybind11::none().
    StateSearch(scenario_run::ScenarioRun& run, pybind11::object handle,
                pybind11::object apply, pybind11::object score,
                pybind11::object restored);

    // The first candidate with the highest score; the run is left as it was.
    [[nodiscard]] auto Best(pybind11::object const& candidates,
                            std::uint32_t depth) -> SearchResult;

  private:
    struct Choice
    {
      pybind11::object candidate{ pybind11::none() };
      pybind11::object score{ pybind11::none() };
      bool             taken{ false };
    };

    [[nodiscard]] auto Listed(pybind11::object const& candidates) const
      -> pybind11::list;

    [[nodiscard]] auto Explore(pybind11::object const& candidates,
                               std::uint32_t depth, std::uint32_t level)
      -> Choice;

    // The one way back to a level's seed: restore, then tell the scenario.
    auto Seed(std::string const& name) -> void;

    [[nodiscard]] auto NameAt(std::uint32_t level) const -> std::string;

    scenario_run::ScenarioRun* _run;

    // The same run as Python hands it to apply and score.
    pybind11::object _handle;
    pybind11::object _apply;
    pybind11::object _score;
    pybind11::object _restored;
    std::uint64_t    _trials{ 0 };

    // This call's own number, which its checkpoint names carry.
    std::uint64_t    _searching{ 0 };
  };
}

namespace tash::python
{
  using detail::state_search::SearchResult;
  using detail::state_search::StateSearch;
}
