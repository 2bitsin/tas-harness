#include "tash/plan/graph-route.hpp"

#include "tash/plan/cost-grid.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <queue>
#include <utility>

namespace tash::plan::detail::graph_route
{
  using utilities::Refused;

  namespace
  {
    struct Reached
    {
      std::uint64_t walked{};
      std::size_t   index{};

      [[nodiscard]] auto operator>(Reached const& other) const noexcept -> bool
      {
        return walked > other.walked;
      }
    };

    using Frontier = std::priority_queue<Reached, std::vector<Reached>,
                                         std::greater<Reached>>;

    [[nodiscard]] auto Walked(Graph const& graph,
                              std::vector<std::size_t> const& came_from,
                              std::size_t start, std::size_t goal)
      -> std::vector<NodeId>
    {
      std::vector<NodeId> nodes;
      for (std::size_t at{ goal }; at != start; at = came_from[at])
        nodes.push_back(graph.IdAt(at));
      nodes.push_back(graph.IdAt(start));
      std::ranges::reverse(nodes);
      return nodes;
    }
  }

  auto GraphRoute(Graph const& graph, NodeId start, NodeId goal)
    -> Result<Walk>
  {
    if (!graph.Holds(start) || !graph.Holds(goal))
      return Refused("plan: {} to {} leaves a graph of {} nodes", start, goal,
                     graph.Count());

    std::size_t const from{ graph.IndexOf(start) };
    std::size_t const to{ graph.IndexOf(goal) };
    std::vector<std::uint64_t> walked(graph.Count(), UNREACHED);
    std::vector<std::size_t>   came_from(graph.Count(), 0u);
    Frontier                   frontier;
    walked[from] = 0u;
    frontier.push(Reached{ 0u, from });

    while (!frontier.empty())
    {
      Reached const here{ frontier.top() };
      frontier.pop();
      if (here.index == to)
        return Walk{ Walked(graph, came_from, from, to), walked[to] };
      if (here.walked > walked[here.index])
        continue;
      for (Step const& next : graph.From(here.index))
      {
        std::uint64_t const cost{ here.walked + next.cost };
        if (cost >= walked[next.to])
          continue;
        walked[next.to] = cost;
        came_from[next.to] = here.index;
        frontier.push(Reached{ cost, next.to });
      }
    }
    return Refused("plan: nothing leads from {} to {}", start, goal);
  }
}
