#include "tash/clock/clock.hpp"
#include "tash/clock/determinism.hpp"

#include <gtest/gtest.h>

namespace
{
  using tash::clock::Clock;
  using tash::clock::Pacing;
  using tash::clock::RATE_REAL_TIME;
  using tash::clock::RATE_UNLIMITED;

  constexpr double FRAME_SECONDS{ 1.0 / 60.0 };
}

TEST(ClockTest, SteppedWaitsForNothing)
{
  Clock clock{ Clock::Stepped() };
  clock.Start(FRAME_SECONDS);
  EXPECT_EQ(clock.Mode(), Pacing::Stepped);

  clock.Await(600);
  EXPECT_LT(clock.WallSeconds(), 1.0);
}

TEST(ClockTest, AnUnlimitedRateIsNotAPace)
{
  Clock clock{ Clock::Paced(RATE_UNLIMITED) };
  clock.Start(FRAME_SECONDS);
  clock.Await(600);
  EXPECT_LT(clock.WallSeconds(), 1.0);
}

TEST(ClockTest, APaceHoldsTheFrameBack)
{
  Clock clock{ Clock::Paced(RATE_REAL_TIME) };
  clock.Start(FRAME_SECONDS);
  clock.Await(3);
  EXPECT_GE(clock.WallSeconds(), 3 * FRAME_SECONDS);
}

TEST(ClockTest, TheAchievedRateIsEmulatedSecondsOverWallSeconds)
{
  Clock clock{ Clock::Paced(RATE_REAL_TIME) };
  clock.Start(FRAME_SECONDS);
  clock.Await(6);
  EXPECT_NEAR(clock.AchievedRate(6), 1.0, 0.5);
  EXPECT_EQ(clock.AchievedRate(0), 0.0);
}

TEST(ClockTest, ADeterminismLevelIsPrintedAsThePlanNamesIt)
{
  EXPECT_EQ(tash::clock::Named(tash::clock::Determinism::D2), "D2");
  EXPECT_EQ(tash::clock::Named(tash::clock::Determinism::D3), "D3");
}
