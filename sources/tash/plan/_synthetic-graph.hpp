#pragma once
// Graphs the tests build: the edges as they are written, and Held from the
// grid's own kit.

#include "_synthetic-grid.hpp"

#include "tash/plan/graph.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace tash::plan::testing
{
  [[nodiscard]] inline auto GraphFrom(std::vector<Edge> const& edges) -> Graph
  {
    Result<Graph> made{ Graph::Of(edges) };
    EXPECT_TRUE(made.has_value()) << (made ? std::string{} : made.error());
    return std::move(made.value());
  }
}
