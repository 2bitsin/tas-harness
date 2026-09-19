#include "tash/plan/graph.hpp"

#include <algorithm>
#include <utility>

namespace tash::plan::detail::graph
{
  using utilities::Refused;

  namespace
  {
    [[nodiscard]] auto NodesOf(std::span<Edge const> edges)
      -> std::vector<NodeId>
    {
      std::vector<NodeId> ids;
      ids.reserve(edges.size() * 2u);
      for (Edge const& edge : edges)
      {
        ids.push_back(edge.from);
        ids.push_back(edge.to);
      }
      std::ranges::sort(ids);
      auto const repeated{ std::ranges::unique(ids) };
      ids.erase(repeated.begin(), repeated.end());
      return ids;
    }
  }

  Graph::Graph(std::vector<NodeId> ids, std::vector<std::vector<Step>> out)
  : _ids{ std::move(ids) }, _out{ std::move(out) }
  {
  }

  auto Graph::Of(std::span<Edge const> edges) -> Result<Graph>
  {
    for (Edge const& edge : edges)
      if (edge.cost < 0)
        return Refused("plan: the edge {} to {} costs {}; a cost is never "
                       "negative", edge.from, edge.to, edge.cost);

    std::vector<NodeId> ids{ NodesOf(edges) };
    std::vector<std::vector<Step>> out(ids.size());
    Graph made{ std::move(ids), std::move(out) };
    for (Edge const& edge : edges)
      made._out[made.IndexOf(edge.from)].push_back(
        Step{ made.IndexOf(edge.to),
              static_cast<std::uint64_t>(edge.cost) });
    return made;
  }

  auto Graph::Count() const noexcept -> std::size_t
  {
    return _ids.size();
  }

  auto Graph::Ids() const noexcept -> std::vector<NodeId> const&
  {
    return _ids;
  }

  auto Graph::Holds(NodeId id) const noexcept -> bool
  {
    return IndexOf(id) != Count();
  }

  auto Graph::IndexOf(NodeId id) const noexcept -> std::size_t
  {
    auto const found{ std::ranges::lower_bound(_ids, id) };
    if (found == _ids.end() || *found != id)
      return Count();
    return static_cast<std::size_t>(found - _ids.begin());
  }

  auto Graph::IdAt(std::size_t index) const noexcept -> NodeId
  {
    return index < _ids.size() ? _ids[index] : NodeId{};
  }

  auto Graph::From(std::size_t index) const -> std::span<Step const>
  {
    if (index >= _out.size())
      return {};
    return _out[index];
  }
}
