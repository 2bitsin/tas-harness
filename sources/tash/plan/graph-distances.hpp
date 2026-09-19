#pragma once
// Dijkstra from each source in turn over a weighted directed graph: a row a
// source, a cost a node in the graph's order, UNREACHED where nothing leads.

#include "tash/plan/graph.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tash::plan::detail::graph_distances
{
  using utilities::Result;

  [[nodiscard]] auto GraphDistances(Graph const& graph,
                                    std::span<NodeId const> sources)
    -> Result<std::vector<std::vector<std::uint64_t>>>;
}

namespace tash::plan
{
  using detail::graph_distances::GraphDistances;
}
