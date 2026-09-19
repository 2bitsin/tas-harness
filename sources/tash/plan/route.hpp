#pragma once
// A* from one cell to another over a cost grid: the cells it walks and what
// walking them costs.

#include "tash/plan/cost-grid.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <vector>

namespace tash::plan::detail::route
{
  using utilities::Result;

  struct Path
  {
    std::vector<Cell> cells;

    // Every cell after the start; UNREACHED, with no cells, when none.
    std::uint64_t cost{ UNREACHED };
  };

  [[nodiscard]] auto Route(CostGrid const& grid, Cell start, Cell goal)
    -> Result<Path>;
}

namespace tash::plan
{
  using detail::route::Path;
  using detail::route::Route;
}
