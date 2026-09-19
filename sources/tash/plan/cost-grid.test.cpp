#include "tash/plan/cost-grid.hpp"

#include "_synthetic-grid.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace
{
  using namespace tash::plan;
  using namespace tash::plan::testing;

  constexpr std::uint32_t SIDE{ 3u };
  constexpr std::array<std::uint8_t, SIDE * SIDE> OPEN{ 1u, 1u, 1u, 1u, 1u,
                                                        1u, 1u, 1u, 1u };
}

TEST(CostGrid, RefusesCostsThatAreNotTheGridsOwnCount)
{
  EXPECT_FALSE(CostGrid::Of(OPEN, SIDE, SIDE + 1u, false).has_value());
  EXPECT_FALSE(CostGrid::Of(OPEN, SIDE + 1u, SIDE, false).has_value());
}

TEST(CostGrid, RefusesASideOfNothing)
{
  EXPECT_FALSE(CostGrid::Of({}, 0u, SIDE, false).has_value());
  EXPECT_FALSE(CostGrid::Of({}, SIDE, 0u, false).has_value());
}

TEST(CostGrid, ReadsTheCostsRowByRow)
{
  CostGrid const grid{ GridFrom({ "123", "456" }) };

  EXPECT_EQ(grid.Width(), 3u);
  EXPECT_EQ(grid.Height(), 2u);
  EXPECT_EQ(grid.Count(), 6u);
  EXPECT_EQ(grid.CostAt(grid.IndexOf(Cell{ 2u, 1u })), 6u);
  EXPECT_EQ(grid.CellAt(4u), (Cell{ 1u, 1u }));
}

TEST(CostGrid, HoldsOnlyTheCellsItHasRoomFor)
{
  CostGrid const grid{ GridFrom({ "..", ".." }) };

  EXPECT_TRUE(grid.Holds(Cell{ 1u, 1u }));
  EXPECT_FALSE(grid.Holds(Cell{ 2u, 1u }));
  EXPECT_FALSE(grid.Holds(Cell{ 1u, 2u }));
}

TEST(CostGrid, CheapestIsTheLeastAPassableCellCosts)
{
  EXPECT_EQ(GridFrom({ "39#", "7#4" }).Cheapest(), 3u);
  EXPECT_EQ(GridFrom({ "##", "##" }).Cheapest(), 0u);
}

TEST(CostGrid, TheNeighbourhoodIsFourSidesOrEightCorners)
{
  CostGrid const sides{ GridFrom({ "...", "...", "..." }) };
  CostGrid const corners{ GridFrom({ "...", "...", "..." }, true) };

  EXPECT_EQ(sides.Around(sides.IndexOf(Cell{ 1u, 1u })).size(), 4u);
  EXPECT_EQ(corners.Around(corners.IndexOf(Cell{ 1u, 1u })).size(), 8u);
  EXPECT_EQ(sides.Around(sides.IndexOf(Cell{ 0u, 0u })).size(), 2u);
  EXPECT_EQ(corners.Around(corners.IndexOf(Cell{ 0u, 0u })).size(), 3u);
}

TEST(CostGrid, StepsAreManhattanOnSidesAndChebyshevOnCorners)
{
  CostGrid const sides{ GridFrom({ "....", "....", "....", "...." }) };
  CostGrid const corners{
    GridFrom({ "....", "....", "....", "...." }, true) };
  std::size_t const from{ sides.IndexOf(Cell{ 0u, 0u }) };
  std::size_t const to{ sides.IndexOf(Cell{ 3u, 2u }) };

  EXPECT_EQ(sides.Steps(from, to), 5u);
  EXPECT_EQ(corners.Steps(from, to), 3u);
}
