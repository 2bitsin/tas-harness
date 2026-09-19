#include "tash/python/frame-budget.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace tash::python::detail::frame_budget
{
  namespace
  {
    constexpr std::uint64_t MADE{ 1000 };
    constexpr std::uint64_t LIMIT{ 100 };
    constexpr std::uint64_t ONE_FRAME{ 1 };
  }

  TEST(FrameBudget, WithoutALimitEveryCallHasRoom)
  {
    FrameBudget budget;
    budget.Begin(MADE, UNLIMITED);
    EXPECT_EQ(budget.Limit(), UNLIMITED);
    EXPECT_TRUE(budget.Room(MADE, ONE_FRAME).has_value());
    EXPECT_TRUE(budget.Room(MADE + LIMIT, LIMIT * LIMIT).has_value());
  }

  TEST(FrameBudget, ACallThatWouldPassTheLimitIsRefused)
  {
    FrameBudget budget;
    budget.Begin(MADE, LIMIT);

    EXPECT_EQ(budget.Spent(MADE + LIMIT / 2), LIMIT / 2);
    EXPECT_TRUE(budget.Room(MADE + LIMIT / 2, LIMIT / 2).has_value());

    auto const over{ budget.Room(MADE + LIMIT / 2, LIMIT) };
    ASSERT_FALSE(over.has_value());
    EXPECT_NE(over.error().find("frame budget"), std::string::npos)
      << over.error();
    EXPECT_NE(over.error().find(std::to_string(LIMIT)), std::string::npos)
      << over.error();

    EXPECT_FALSE(budget.Room(MADE + LIMIT, ONE_FRAME).has_value());
  }

  TEST(FrameBudget, AStopRefusesTheNextCallWhateverIsLeft)
  {
    FrameBudget budget;
    budget.Begin(MADE, UNLIMITED);
    budget.Stop();

    auto const stopped{ budget.Room(MADE + ONE_FRAME, ONE_FRAME) };
    ASSERT_FALSE(stopped.has_value());
    EXPECT_NE(stopped.error().find("cancelled"), std::string::npos)
      << stopped.error();

    // The next job starts on its own terms, stop and limit alike.
    budget.Begin(MADE, UNLIMITED);
    EXPECT_TRUE(budget.Room(MADE, ONE_FRAME).has_value());
  }

  TEST(FrameBudget, TheEndOfAJobLeavesNothingCounting)
  {
    FrameBudget budget;
    budget.Begin(MADE, LIMIT);
    budget.End();
    EXPECT_EQ(budget.Limit(), UNLIMITED);
    EXPECT_TRUE(budget.Room(MADE + LIMIT * LIMIT, LIMIT).has_value());
  }

  TEST(FrameBudget, AFrameCountBelowTheStartSpendsNothing)
  {
    FrameBudget budget;
    budget.Begin(MADE, LIMIT);
    EXPECT_EQ(budget.Spent(0), 0u);
    EXPECT_TRUE(budget.Room(0, LIMIT).has_value());
  }
}
