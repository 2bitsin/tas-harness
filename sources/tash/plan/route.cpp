#include "tash/plan/route.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace tash::plan::detail::route
{
  using utilities::Refused;

  namespace
  {
    struct Reached
    {
      std::uint64_t estimate{};
      std::size_t   index{};

      [[nodiscard]] auto operator>(Reached const& other) const noexcept -> bool
      {
        return estimate > other.estimate;
      }
    };

    using Frontier = std::priority_queue<Reached, std::vector<Reached>,
                                         std::greater<Reached>>;

    // Every remaining step costs at least the grid's cheapest cell, so this
    // never overstates the way out and A* stays optimal on either
    // neighbourhood.
    [[nodiscard]] auto Ahead(CostGrid const& grid, std::size_t from,
                             std::size_t goal) -> std::uint64_t
    {
      return grid.Steps(from, goal) * grid.Cheapest();
    }

    [[nodiscard]] auto Walked(std::vector<std::size_t> const& came_from,
                              CostGrid const& grid, std::size_t start,
                              std::size_t goal) -> std::vector<Cell>
    {
      std::vector<Cell> cells;
      for (std::size_t at{ goal }; at != start; at = came_from[at])
        cells.push_back(grid.CellAt(at));
      cells.push_back(grid.CellAt(start));
      std::ranges::reverse(cells);
      return cells;
    }
  }

  auto Route(CostGrid const& grid, Cell start, Cell goal) -> Result<Path>
  {
    if (!grid.Holds(start) || !grid.Holds(goal))
      return Refused("plan: ({},{}) to ({},{}) leaves a {} by {} grid",
                     start.x, start.y, goal.x, goal.y, grid.Width(),
                     grid.Height());

    std::size_t const from{ grid.IndexOf(start) };
    std::size_t const to{ grid.IndexOf(goal) };
    if (!grid.Passable(from) || !grid.Passable(to))
      return Path{ };

    std::vector<std::uint64_t> walked(grid.Count(), UNREACHED);
    std::vector<std::size_t>   came_from(grid.Count(), 0u);
    Frontier                   frontier;
    walked[from] = 0u;
    frontier.push(Reached{ Ahead(grid, from, to), from });

    while (!frontier.empty())
    {
      Reached const here{ frontier.top() };
      frontier.pop();
      if (here.index == to)
        return Path{ Walked(came_from, grid, from, to), walked[to] };
      if (here.estimate > walked[here.index] + Ahead(grid, here.index, to))
        continue;
      for (std::size_t next : grid.Around(here.index))
      {
        if (!grid.Passable(next))
          continue;
        std::uint64_t const cost{ walked[here.index] + grid.CostAt(next) };
        if (cost >= walked[next])
          continue;
        walked[next] = cost;
        came_from[next] = here.index;
        frontier.push(Reached{ cost + Ahead(grid, next, to), next });
      }
    }
    return Path{ };
  }
}
