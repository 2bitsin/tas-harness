#include "tash/plan/distances.hpp"

#include "_synthetic-grid.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace
{
  using namespace tash::plan;
  using namespace tash::plan::testing;

  using Spread = std::vector<std::uint64_t>;

  [[nodiscard]] auto At(CostGrid const& grid, Spread const& spread, Cell cell)
    -> std::uint64_t
  {
    return spread[grid.IndexOf(cell)];
  }
}

TEST(Distances, ASourceCostsNothingAndItsNeighbourCostsItsOwnEntry)
{
  CostGrid const grid{ GridFrom({ "141", "414", "141" }) };

  Spread const spread{
    Held(Distances(grid, std::vector<Cell>{ Cell{ 1u, 1u } })) };

  EXPECT_EQ(At(grid, spread, Cell{ 1u, 1u }), 0u);
  EXPECT_EQ(At(grid, spread, Cell{ 1u, 0u }), 4u);
  EXPECT_EQ(At(grid, spread, Cell{ 0u, 1u }), 4u);
  EXPECT_EQ(At(grid, spread, Cell{ 0u, 0u }), 5u);
}

TEST(Distances, EveryCellTakesTheNearestOfTwoSources)
{
  CostGrid const grid{ GridFrom({ "........." }) };

  Spread const spread{ Held(Distances(
    grid, std::vector<Cell>{ Cell{ 0u, 0u }, Cell{ 8u, 0u } })) };

  EXPECT_EQ(At(grid, spread, Cell{ 1u, 0u }), 1u);
  EXPECT_EQ(At(grid, spread, Cell{ 4u, 0u }), 4u);
  EXPECT_EQ(At(grid, spread, Cell{ 7u, 0u }), 1u);
}

TEST(Distances, WhatAWallShutsOffIsNeverReached)
{
  CostGrid const grid{ GridFrom({ ".#.", ".#.", ".#." }) };

  Spread const spread{
    Held(Distances(grid, std::vector<Cell>{ Cell{ 0u, 0u } })) };

  EXPECT_EQ(At(grid, spread, Cell{ 0u, 2u }), 2u);
  EXPECT_EQ(At(grid, spread, Cell{ 1u, 0u }), UNREACHED);
  EXPECT_EQ(At(grid, spread, Cell{ 2u, 0u }), UNREACHED);
}

TEST(Distances, AnImpassableSourceIsNoSource)
{
  CostGrid const grid{ GridFrom({ "#..", "...", "..." }) };

  Spread const spread{
    Held(Distances(grid, std::vector<Cell>{ Cell{ 0u, 0u } })) };

  EXPECT_EQ(At(grid, spread, Cell{ 0u, 0u }), UNREACHED);
  EXPECT_EQ(At(grid, spread, Cell{ 2u, 2u }), UNREACHED);
}

TEST(Distances, NoSourceLeavesEveryCellUnreached)
{
  CostGrid const grid{ GridFrom({ "..", ".." }) };

  Spread const spread{ Held(Distances(grid, std::vector<Cell>{})) };

  EXPECT_EQ(spread.size(), grid.Count());
  for (std::uint64_t reach : spread)
    EXPECT_EQ(reach, UNREACHED);
}

TEST(Distances, TheCornerNeighbourhoodReachesADiagonalInOneStep)
{
  std::vector<std::string_view> const drawn{ "...", "...", "..." };
  std::vector<Cell> const sources{ Cell{ 0u, 0u } };

  CostGrid const sides{ GridFrom(drawn) };
  CostGrid const corners{ GridFrom(drawn, true) };

  EXPECT_EQ(At(sides, Held(Distances(sides, sources)), Cell{ 2u, 2u }), 4u);
  EXPECT_EQ(At(corners, Held(Distances(corners, sources)), Cell{ 2u, 2u }),
            2u);
}

TEST(Distances, RefusesASourceThatLeavesTheGrid)
{
  CostGrid const grid{ GridFrom({ "..", ".." }) };

  EXPECT_FALSE(
    Distances(grid, std::vector<Cell>{ Cell{ 2u, 0u } }).has_value());
}
