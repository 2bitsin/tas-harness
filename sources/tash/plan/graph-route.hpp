#pragma once
// Dijkstra from one node to another over a weighted directed graph: the
// nodes it walks and what walking them costs.

#include "tash/plan/graph.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <vector>

namespace tash::plan::detail::graph_route
{
  using utilities::Result;

  struct Walk
  {
    std::vector<NodeId> nodes;
    std::uint64_t       cost{ 0 };
  };

  // A node the graph has not got refuses, and so does a goal nothing leads to.
  [[nodiscard]] auto GraphRoute(Graph const& graph, NodeId start, NodeId goal)
    -> Result<Walk>;
}

namespace tash::plan
{
  using detail::graph_route::GraphRoute;
  using detail::graph_route::Walk;
}
