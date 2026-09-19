#include "tash/plan/graph-distances.hpp"

#include "tash/plan/cost-grid.hpp"

#include <cstddef>
#include <functional>
#include <queue>

namespace tash::plan::detail::graph_distances
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

    [[nodiscard]] auto Spread(Graph const& graph, std::size_t from)
      -> std::vector<std::uint64_t>
    {
      std::vector<std::uint64_t> walked(graph.Count(), UNREACHED);
      Frontier                   frontier;
      walked[from] = 0u;
      frontier.push(Reached{ 0u, from });

      while (!frontier.empty())
      {
        Reached const here{ frontier.top() };
        frontier.pop();
        if (here.walked > walked[here.index])
          continue;
        for (Step const& next : graph.From(here.index))
        {
          std::uint64_t const cost{ here.walked + next.cost };
          if (cost >= walked[next.to])
            continue;
          walked[next.to] = cost;
          frontier.push(Reached{ cost, next.to });
        }
      }
      return walked;
    }
  }

  auto GraphDistances(Graph const& graph, std::span<NodeId const> sources)
    -> Result<std::vector<std::vector<std::uint64_t>>>
  {
    std::vector<std::vector<std::uint64_t>> table;
    table.reserve(sources.size());
    for (NodeId source : sources)
    {
      if (!graph.Holds(source))
        return Refused("plan: source {} leaves a graph of {} nodes", source,
                       graph.Count());
      table.push_back(Spread(graph, graph.IndexOf(source)));
    }
    return table;
  }
}
