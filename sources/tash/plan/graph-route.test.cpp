#include "tash/plan/graph-route.hpp"

#include "_synthetic-graph.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{
  using namespace tash::plan;
  using namespace tash::plan::testing;

  // The lobby, the hallway, the cafe and the office, as room words.
  auto Rooms() -> Graph
  {
    return GraphFrom({ Edge{ 16, 9, 64 }, Edge{ 9, 16, 64 },
                       Edge{ 16, 13, 176 }, Edge{ 13, 9, 20 },
                       Edge{ 16, 17, 12 } });
  }
}

TEST(GraphRoute, WalksTheCheaperOfTwoWaysRound)
{
  Walk const walk{ Held(GraphRoute(Rooms(), 16, 9)) };

  EXPECT_EQ(walk.nodes, (std::vector<NodeId>{ 16, 9 }));
  EXPECT_EQ(walk.cost, 64u);
}

TEST(GraphRoute, TakesTwoEdgesWhenTheyCostLessThanOne)
{
  Graph const graph{ GraphFrom(
    { Edge{ 1, 2, 3 }, Edge{ 2, 3, 4 }, Edge{ 1, 3, 20 } }) };

  Walk const walk{ Held(GraphRoute(graph, 1, 3)) };

  EXPECT_EQ(walk.nodes, (std::vector<NodeId>{ 1, 2, 3 }));
  EXPECT_EQ(walk.cost, 7u);
}

TEST(GraphRoute, AnEdgeLeadsOneWayOnly)
{
  Graph const graph{ GraphFrom({ Edge{ 16, 17, 12 } }) };

  EXPECT_TRUE(GraphRoute(graph, 16, 17).has_value());
  EXPECT_FALSE(GraphRoute(graph, 17, 16).has_value());
}

TEST(GraphRoute, TheStartIsItsOwnGoalAtNoCost)
{
  Walk const walk{ Held(GraphRoute(Rooms(), 13, 13)) };

  EXPECT_EQ(walk.nodes, (std::vector<NodeId>{ 13 }));
  EXPECT_EQ(walk.cost, 0u);
}

TEST(GraphRoute, RefusesANodeTheGraphHasNotGot)
{
  EXPECT_FALSE(GraphRoute(Rooms(), 16, 21).has_value());
  EXPECT_FALSE(GraphRoute(Rooms(), 21, 16).has_value());
}

TEST(GraphRoute, RefusesAGoalNothingLeadsTo)
{
  Graph const graph{ GraphFrom({ Edge{ 1, 2, 1 }, Edge{ 3, 2, 1 } }) };

  EXPECT_FALSE(GraphRoute(graph, 1, 3).has_value());
}
