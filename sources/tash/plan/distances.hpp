#pragma once
// Multi-source Dijkstra over a cost grid: the reach of the nearest source.

#include "tash/plan/cost-grid.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tash::plan::detail::distances
{
  using utilities::Result;

  // One cost a cell row by row; UNREACHED where no passable source reaches.
  [[nodiscard]] auto Distances(CostGrid const& grid,
                               std::span<Cell const> sources)
    -> Result<std::vector<std::uint64_t>>;
}

namespace tash::plan
{
  using detail::distances::Distances;
}
