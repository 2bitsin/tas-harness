#include "tash/plan/graph-distances.hpp"

#include "_synthetic-graph.hpp"

#include "tash/plan/cost-grid.hpp"

#include <cstdint>
#include <gtest/gtest.h>

#include <vector>

namespace
{
  using namespace tash::plan;
  using namespace tash::plan::testing;

  using Table = std::vector<std::vector<std::uint64_t>>;

  auto Chain() -> Graph
  {
    return GraphFrom({ Edge{ 1, 2, 3 }, Edge{ 2, 3, 4 }, Edge{ 1, 3, 20 } });
  }

  [[nodiscard]] auto At(Graph const& graph,
                        std::vector<std::uint64_t> const& row, NodeId node)
    -> std::uint64_t
  {
    return row[graph.IndexOf(node)];
  }
}

TEST(GraphDistances, OneRowASourceInTheGraphsOwnNodeOrder)
{
  Graph const graph{ Chain() };

  Table const table{
    Held(GraphDistances(graph, std::vector<NodeId>{ 1, 2 })) };

  ASSERT_EQ(table.size(), 2u);
  EXPECT_EQ(table[0].size(), graph.Count());
  EXPECT_EQ(At(graph, table[0], 1), 0u);
  EXPECT_EQ(At(graph, table[0], 2), 3u);
  EXPECT_EQ(At(graph, table[0], 3), 7u);
  EXPECT_EQ(At(graph, table[1], 3), 4u);
}

TEST(GraphDistances, WhatASourceDoesNotLeadToIsNeverReached)
{
  Graph const graph{ Chain() };

  Table const table{ Held(GraphDistances(graph, std::vector<NodeId>{ 3 })) };

  EXPECT_EQ(At(graph, table[0], 3), 0u);
  EXPECT_EQ(At(graph, table[0], 1), UNREACHED);
  EXPECT_EQ(At(graph, table[0], 2), UNREACHED);
}

TEST(GraphDistances, NoSourceIsAnEmptyTable)
{
  EXPECT_TRUE(Held(GraphDistances(Chain(), std::vector<NodeId>{})).empty());
}

TEST(GraphDistances, RefusesASourceTheGraphHasNotGot)
{
  EXPECT_FALSE(
    GraphDistances(Chain(), std::vector<NodeId>{ 4 }).has_value());
}
