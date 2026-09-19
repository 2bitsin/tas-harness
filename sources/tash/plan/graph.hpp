#pragma once
// A weighted directed graph given as its edges, with node ids a caller
// chooses: the model a plan is searched over when the moves are not a grid.

#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tash::plan::detail::graph
{
  using utilities::Result;

  using NodeId = std::int64_t;

  struct Edge
  {
    NodeId       from{};
    NodeId       to{};
    std::int64_t cost{};
  };

  // One way out of a node: where it lands, as an index, and what it costs.
  struct Step
  {
    std::size_t   to{};
    std::uint64_t cost{};
  };

  class Graph
  {
  public:
    [[nodiscard]] static auto Of(std::span<Edge const> edges) -> Result<Graph>;

    [[nodiscard]] auto Count() const noexcept -> std::size_t;

    // The nodes in ascending id order, which is the order every answer is in.
    [[nodiscard]] auto Ids() const noexcept -> std::vector<NodeId> const&;

    [[nodiscard]] auto Holds(NodeId id) const noexcept -> bool;

    // Count() when the graph has no node of that id.
    [[nodiscard]] auto IndexOf(NodeId id) const noexcept -> std::size_t;

    [[nodiscard]] auto IdAt(std::size_t index) const noexcept -> NodeId;

    [[nodiscard]] auto From(std::size_t index) const -> std::span<Step const>;

  private:
    Graph(std::vector<NodeId> ids, std::vector<std::vector<Step>> out);

    std::vector<NodeId>            _ids;
    std::vector<std::vector<Step>> _out;
  };
}

namespace tash::plan
{
  using detail::graph::Edge;
  using detail::graph::Graph;
  using detail::graph::NodeId;
  using detail::graph::Step;
}
