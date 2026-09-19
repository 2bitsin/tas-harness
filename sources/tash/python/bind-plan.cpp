#include "tash/python/bindings.hpp"

#include "tash/plan/cost-grid.hpp"
#include "tash/plan/distances.hpp"
#include "tash/plan/graph-distances.hpp"
#include "tash/plan/graph-route.hpp"
#include "tash/plan/graph.hpp"
#include "tash/plan/route.hpp"

#include <pybind11/stl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  namespace
  {
    using Point = std::array<std::uint32_t, 2>;
    using Line = std::array<plan::NodeId, 3>;

    constexpr char const* PLAN_DOC{
      "The classic searches, over a cost grid and over a weighted directed "
      "graph: A* and Dijkstra for one route, Dijkstra for how far every "
      "cell or node is from a source." };

    auto CellOf(Point const& point) -> plan::Cell
    {
      return plan::Cell{ point[0], point[1] };
    }

    auto GridOf(py::bytes const& costs, std::uint32_t width,
                std::uint32_t height, bool diagonal) -> plan::CostGrid
    {
      std::string const held{ costs.cast<std::string>() };
      return Given(plan::CostGrid::Of(
        { reinterpret_cast<std::uint8_t const*>(held.data()), held.size() },
        width, height, diagonal));
    }

    auto Walked(py::bytes const& costs, std::uint32_t width,
                std::uint32_t height, Point const& start, Point const& goal,
                bool diagonal) -> py::dict
    {
      plan::Path const path{ Given(plan::Route(
        GridOf(costs, width, height, diagonal), CellOf(start),
        CellOf(goal))) };
      py::list cells;
      for (plan::Cell const& cell : path.cells)
        cells.append(py::make_tuple(cell.x, cell.y));
      py::dict answer;
      answer["cells"] = cells;
      answer["cost"] = path.cost;
      return answer;
    }

    auto GraphOf(std::vector<Line> const& edges) -> plan::Graph
    {
      std::vector<plan::Edge> written;
      written.reserve(edges.size());
      for (Line const& edge : edges)
        written.push_back(plan::Edge{ edge[0], edge[1], edge[2] });
      return Given(plan::Graph::Of(written));
    }

    auto Led(std::vector<Line> const& edges, plan::NodeId start,
             plan::NodeId goal) -> py::dict
    {
      plan::Walk const walk{
        Given(plan::GraphRoute(GraphOf(edges), start, goal)) };
      py::dict answer;
      answer["nodes"] = py::cast(walk.nodes);
      answer["cost"] = walk.cost;
      return answer;
    }

    auto Table(std::vector<Line> const& edges,
               std::vector<plan::NodeId> const& sources) -> py::dict
    {
      plan::Graph const graph{ GraphOf(edges) };
      std::vector<std::vector<std::uint64_t>> const spread{
        Given(plan::GraphDistances(graph, sources)) };
      py::dict table;
      for (std::size_t source{ 0 }; source < sources.size(); ++source)
      {
        py::dict reach;
        for (std::size_t node{ 0 }; node < graph.Count(); ++node)
          reach[py::cast(graph.IdAt(node))] = spread[source][node];
        table[py::cast(sources[source])] = reach;
      }
      return table;
    }

    auto Spread(py::bytes const& costs, std::uint32_t width,
                std::uint32_t height, std::vector<Point> const& sources,
                bool diagonal) -> std::vector<std::uint64_t>
    {
      std::vector<plan::Cell> from;
      from.reserve(sources.size());
      for (Point const& point : sources)
        from.push_back(CellOf(point));
      return Given(plan::Distances(GridOf(costs, width, height, diagonal),
                                   from));
    }
  }

  auto BindPlan(py::module_& module) -> void
  {
    py::module_ planner{ module.def_submodule("plan", PLAN_DOC) };
    planner.attr("IMPASSABLE") = plan::IMPASSABLE;
    planner.attr("UNREACHED") = plan::UNREACHED;

    planner.def("route", &Walked, py::arg("costs"), py::arg("width"),
                py::arg("height"), py::arg("start"), py::arg("goal"),
                py::arg("diagonal") = false);

    planner.def("distances", &Spread, py::arg("costs"), py::arg("width"),
                py::arg("height"), py::arg("sources"),
                py::arg("diagonal") = false);

    planner.def("graph", &Led, py::arg("edges"), py::arg("start"),
                py::arg("goal"));

    planner.def("graph_distances", &Table, py::arg("edges"),
                py::arg("sources"));
  }
}
