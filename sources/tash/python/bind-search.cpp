#include "tash/python/bindings.hpp"

#include "tash/python/state-search.hpp"

#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using state_search::SearchResult;
  using state_search::StateSearch;

  namespace
  {
    constexpr char const* RESULT_TYPE{ "SearchResult" };

    auto Worded(SearchResult const& answer) -> std::string
    {
      return std::format(
        "SearchResult(best={}, score={}, trials={}, frames={}, seconds={:.3f})",
        py::repr(answer.best).cast<std::string>(),
        py::repr(answer.score).cast<std::string>(), answer.trials,
        answer.frames, answer.seconds);
    }
  }

  auto BindSearch(py::module_& module) -> void
  {
    py::class_<SearchResult>{ module, RESULT_TYPE }
      .def_readonly("best", &SearchResult::best)
      .def_readonly("score", &SearchResult::score)
      .def_readonly("trials", &SearchResult::trials)
      .def_readonly("frames", &SearchResult::frames)
      .def_readonly("seconds", &SearchResult::seconds)
      .def("__repr__", &Worded);

    module.def("search",
               [module](py::object const& candidates, py::object apply,
                        py::object score, std::uint32_t depth,
                        py::object restored)
               {
                 StateSearch searching{ RunOf(module),
                                        module.attr(RUN_NAME),
                                        std::move(apply), std::move(score),
                                        std::move(restored) };
                 return searching.Best(candidates, depth);
               },
               py::arg("candidates"), py::arg("apply"), py::arg("score"),
               py::arg("depth") = 1, py::arg("restored") = py::none());
  }
}
