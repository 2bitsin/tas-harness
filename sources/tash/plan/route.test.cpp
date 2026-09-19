#include "tash/plan/route.hpp"

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

  constexpr Cell CORNER{ 0u, 0u };
  constexpr Cell FAR_CORNER{ 4u, 4u };
}

TEST(Route, WalksAnOpenGridFromStartToGoal)
{
  CostGrid const grid{ GridFrom({ ".....", ".....", ".....", ".....",
                                  "....." }) };

  Path const walked{ Held(Route(grid, CORNER, FAR_CORNER)) };

  EXPECT_EQ(walked.cells.front(), CORNER);
  EXPECT_EQ(walked.cells.back(), FAR_CORNER);
  EXPECT_EQ(walked.cells.size(), 9u);
  EXPECT_EQ(walked.cost, 8u);
}

TEST(Route, TheCostIsEveryCellEnteredAndNotTheStart)
{
  CostGrid const grid{ GridFrom({ "911", "999", "999" }) };

  Path const walked{ Held(Route(grid, CORNER, Cell{ 2u, 0u })) };

  EXPECT_EQ(walked.cost, 2u);
}

TEST(Route, StandingOnTheGoalCostsNothing)
{
  CostGrid const grid{ GridFrom({ "..", ".." }) };

  Path const walked{ Held(Route(grid, CORNER, CORNER)) };

  EXPECT_EQ(walked.cells, std::vector<Cell>{ CORNER });
  EXPECT_EQ(walked.cost, 0u);
}

TEST(Route, GoesRoundAWallRatherThanThroughIt)
{
  CostGrid const grid{ GridFrom({ "..#..", "..#..", "..#..", "..#..",
                                  "....." }) };

  Path const walked{ Held(Route(grid, CORNER, Cell{ 4u, 0u })) };

  EXPECT_EQ(walked.cost, 12u);
  for (Cell const& cell : walked.cells)
    EXPECT_TRUE(grid.Passable(grid.IndexOf(cell))) << "a wall was walked";
}

TEST(Route, TakesTheCheapWayRoundOverTheShortExpensiveOne)
{
  CostGrid const grid{ GridFrom({ "9991", "1991", "1111" }) };

  Path const walked{ Held(Route(grid, CORNER, Cell{ 3u, 0u })) };

  EXPECT_EQ(walked.cost, 7u);
  EXPECT_EQ(walked.cells.size(), 8u);
}

TEST(Route, TheCornerNeighbourhoodCutsTheDiagonal)
{
  std::vector<std::string_view> const drawn{ ".....", ".....", ".....",
                                             ".....", "....." };

  Path const sides{ Held(Route(GridFrom(drawn), CORNER, FAR_CORNER)) };
  Path const corners{ Held(Route(GridFrom(drawn, true), CORNER,
                                 FAR_CORNER)) };

  EXPECT_EQ(sides.cost, 8u);
  EXPECT_EQ(corners.cost, 4u);
  EXPECT_EQ(corners.cells.size(), 5u);
}

TEST(Route, AnsweringNoRouteRatherThanRefusingWhenTheWallIsWhole)
{
  CostGrid const grid{ GridFrom({ "..#..", "..#..", "..#.." }) };

  Path const walked{ Held(Route(grid, CORNER, Cell{ 4u, 0u })) };

  EXPECT_TRUE(walked.cells.empty());
  EXPECT_EQ(walked.cost, UNREACHED);
}

TEST(Route, AnImpassableStartOrGoalIsNoRoute)
{
  CostGrid const grid{ GridFrom({ "#..", "...", "..#" }) };

  EXPECT_EQ(Held(Route(grid, CORNER, Cell{ 1u, 1u })).cost, UNREACHED);
  EXPECT_EQ(Held(Route(grid, Cell{ 1u, 1u }, Cell{ 2u, 2u })).cost, UNREACHED);
}

TEST(Route, RefusesACellThatLeavesTheGrid)
{
  CostGrid const grid{ GridFrom({ "..", ".." }) };

  EXPECT_FALSE(Route(grid, Cell{ 2u, 0u }, CORNER).has_value());
  EXPECT_FALSE(Route(grid, CORNER, Cell{ 0u, 2u }).has_value());
}

TEST(Route, AgreesWithDijkstraOnAMixedGrid)
{
  std::vector<std::string_view> const drawn{
    "1932#41", "5#12391", "1119#31", "93#1141", "1141#91", "7#31119" };

  for (bool diagonal : { false, true })
  {
    CostGrid const grid{ GridFrom(drawn, diagonal) };
    std::vector<std::uint64_t> const spread{
      Held(Distances(grid, std::vector<Cell>{ CORNER })) };
    for (std::size_t index{ 0 }; index < grid.Count(); ++index)
      EXPECT_EQ(Held(Route(grid, CORNER, grid.CellAt(index))).cost,
                spread[index])
        << "cell " << index << (diagonal ? " with corners" : " with sides");
  }
}
