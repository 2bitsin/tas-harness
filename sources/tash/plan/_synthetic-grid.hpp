#pragma once
// Grids the tests draw: '#' is impassable, '.' costs one and a digit costs
// what it reads.

#include "tash/plan/cost-grid.hpp"

#include "tash/utilities/outcome.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tash::plan::testing
{
  using utilities::Result;

  inline constexpr char WALL{ '#' };
  inline constexpr char PLAIN{ '.' };
  inline constexpr std::uint8_t PLAIN_COST{ 1u };

  template <typename Value>
  [[nodiscard]] auto Held(Result<Value> const& result) -> Value
  {
    EXPECT_TRUE(result.has_value())
      << (result ? std::string{} : result.error());
    return result.value_or(Value{});
  }

  [[nodiscard]] inline auto CostsOf(std::vector<std::string_view> const& rows)
    -> std::vector<std::uint8_t>
  {
    std::vector<std::uint8_t> costs;
    for (std::string_view row : rows)
      for (char drawn : row)
        costs.push_back(drawn == WALL    ? IMPASSABLE
                        : drawn == PLAIN ? PLAIN_COST
                                         : static_cast<std::uint8_t>(drawn
                                                                     - '0'));
    return costs;
  }

  // A drawing that is not rectangular refuses inside Of, which throws here.
  [[nodiscard]] inline auto GridFrom(std::vector<std::string_view> const& rows,
                                     bool diagonal = false) -> CostGrid
  {
    std::vector<std::uint8_t> const costs{ CostsOf(rows) };
    Result<CostGrid> made{ CostGrid::Of(
      costs, static_cast<std::uint32_t>(rows.empty() ? 0u : rows[0].size()),
      static_cast<std::uint32_t>(rows.size()), diagonal) };
    return std::move(made.value());
  }
}
