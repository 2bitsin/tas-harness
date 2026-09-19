#include "tash/plan/distances.hpp"

#include <cstddef>
#include <functional>
#include <queue>
#include <vector>

namespace tash::plan::detail::distances
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
  }

  auto Distances(CostGrid const& grid, std::span<Cell const> sources)
    -> Result<std::vector<std::uint64_t>>
  {
    std::vector<std::uint64_t> walked(grid.Count(), UNREACHED);
    Frontier                   frontier;
    for (Cell const& source : sources)
    {
      if (!grid.Holds(source))
        return Refused("plan: source ({},{}) leaves a {} by {} grid",
                       source.x, source.y, grid.Width(), grid.Height());
      std::size_t const at{ grid.IndexOf(source) };
      if (!grid.Passable(at) || walked[at] == 0u)
        continue;
      walked[at] = 0u;
      frontier.push(Reached{ 0u, at });
    }

    while (!frontier.empty())
    {
      Reached const here{ frontier.top() };
      frontier.pop();
      if (here.walked > walked[here.index])
        continue;
      for (std::size_t next : grid.Around(here.index))
      {
        if (!grid.Passable(next))
          continue;
        std::uint64_t const cost{ here.walked + grid.CostAt(next) };
        if (cost >= walked[next])
          continue;
        walked[next] = cost;
        frontier.push(Reached{ cost, next });
      }
    }
    return walked;
  }
}
