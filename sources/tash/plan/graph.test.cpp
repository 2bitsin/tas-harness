#include "tash/plan/graph.hpp"

#include "_synthetic-graph.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{
  using namespace tash::plan;
  using namespace tash::plan::testing;
}

TEST(Graph, ItsNodesAreBothEndsOfEveryEdge)
{
  Graph const graph{ GraphFrom({ Edge{ 16, 9, 1 }, Edge{ 9, 2, 1 } }) };

  EXPECT_EQ(graph.Count(), 3u);
  EXPECT_EQ(graph.Ids(), (std::vector<NodeId>{ 2, 9, 16 }));
  EXPECT_EQ(graph.IdAt(0u), 2);
}

TEST(Graph, ANodeItHasNotGotIsNeitherHeldNorIndexed)
{
  Graph const graph{ GraphFrom({ Edge{ 1, 2, 1 } }) };

  EXPECT_TRUE(graph.Holds(1));
  EXPECT_FALSE(graph.Holds(3));
  EXPECT_EQ(graph.IndexOf(3), graph.Count());
}

TEST(Graph, EveryEdgeLeavesTheNodeItWasWrittenFrom)
{
  Graph const graph{ GraphFrom(
    { Edge{ 1, 2, 5 }, Edge{ 1, 3, 7 }, Edge{ 2, 1, 9 } }) };

  EXPECT_EQ(graph.From(graph.IndexOf(1)).size(), 2u);
  EXPECT_EQ(graph.From(graph.IndexOf(2)).size(), 1u);
  EXPECT_EQ(graph.From(graph.IndexOf(3)).size(), 0u);
  EXPECT_EQ(graph.From(graph.IndexOf(2))[0].cost, 9u);
  EXPECT_EQ(graph.From(graph.IndexOf(2))[0].to, graph.IndexOf(1));
}

TEST(Graph, NoEdgesIsAGraphOfNoNodes)
{
  Graph const graph{ GraphFrom({}) };

  EXPECT_EQ(graph.Count(), 0u);
  EXPECT_TRUE(graph.From(0u).empty());
}

TEST(Graph, RefusesAnEdgeThatCostsLessThanNothing)
{
  EXPECT_FALSE(Graph::Of(std::vector<Edge>{ Edge{ 1, 2, -1 } }).has_value());
}
