#include "tash/python/state-search.hpp"

#include "tash/python/bindings.hpp"

#include <chrono>
#include <format>
#include <stdexcept>
#include <utility>

namespace tash::python::detail::state_search
{
  namespace py = pybind11;

  using bindings::Given;
  using scenario_run::CheckpointKind;

  StateSearch::StateSearch(scenario_run::ScenarioRun& run, py::object handle,
                           py::object apply, py::object score,
                           py::object restored)
  : _run{ &run }
  , _handle{ std::move(handle) }
  , _apply{ std::move(apply) }
  , _score{ std::move(score) }
  , _restored{ std::move(restored) }
  {
  }

  auto StateSearch::NameAt(std::uint32_t level) const -> std::string
  {
    return std::format("{}{}/{}", CHECKPOINT_PREFIX, _searching, level);
  }

  auto StateSearch::Seed(std::string const& name) -> void
  {
    static_cast<void>(Given(_run->Restore(name)));
    if (!_restored.is_none())
      _restored(_handle);
  }

  auto StateSearch::Listed(py::object const& candidates) const -> py::list
  {
    py::object const resolved{ PyCallable_Check(candidates.ptr()) != 0
                                 ? candidates(_handle)
                                 : candidates };
    PyObject* const made{ PySequence_List(resolved.ptr()) };
    if (made == nullptr)
      throw py::error_already_set{ };
    return py::reinterpret_steal<py::list>(made);
  }

  auto StateSearch::Explore(py::object const& candidates, std::uint32_t depth,
                            std::uint32_t level) -> Choice
  {
    std::string const name{ NameAt(level) };

    // Consumed here, by the restores below, so a probe would only prove the
    // restore this call is about to make, at 2 * PROBE_FRAMES a level.
    static_cast<void>(Given(_run->Checkpoint(name,
                                             CheckpointKind::SCRATCH)));

    Choice best{ };
    try
    {
      for (py::handle held : Listed(candidates))
      {
        py::object const candidate{ py::reinterpret_borrow<py::object>(
          held) };
        Seed(name);
        _apply(_handle, candidate);
        py::object value{ _score(_handle) };
        ++_trials;

        // A candidate is worth its best child when there is a lookahead.
        if (depth > 1)
          if (Choice const deeper{ Explore(candidates, depth - 1, level + 1) };
              deeper.taken)
            value = deeper.score;

        if (!best.taken || value > best.score)
          best = Choice{ candidate, std::move(value), true };
      }
    }
    catch (...)
    {
      try
      {
        Seed(name);
      }
      catch (...)
      {
        // The failure on its way out is the one the scenario asked about.
      }
      _run->Forget(name);
      throw;
    }

    Seed(name);
    _run->Forget(name);
    return best;
  }

  auto StateSearch::Best(py::object const& candidates, std::uint32_t depth)
    -> SearchResult
  {
    if (depth == 0)
      throw std::runtime_error{ "python: search needs a depth of 1 or more" };

    auto const started{ std::chrono::steady_clock::now() };
    std::uint64_t const from{ _run->Live().Frames() };
    _trials = 0;
    _searching = _run->NextSearch();

    Choice const chosen{ Explore(candidates, depth, 0) };

    SearchResult answer;
    answer.best = chosen.candidate;
    answer.score = chosen.score;
    answer.trials = _trials;
    answer.frames = _run->Live().Frames() - from;
    answer.seconds = std::chrono::duration<double>{
      std::chrono::steady_clock::now() - started }.count();
    return answer;
  }
}
