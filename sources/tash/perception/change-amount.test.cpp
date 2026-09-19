#include "tash/perception/change-amount.hpp"

#include "_synthetic-frame.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace tash::perception;
  using namespace tash::perception::testing;

  constexpr std::uint32_t FRAME_WIDTH{ 64u };
  constexpr std::uint32_t FRAME_HEIGHT{ 64u };
  constexpr Region        PATCH{ 8u, 8u, 8u, 8u };
  constexpr Region        MOVED{ 32u, 8u, 8u, 8u };
}

TEST(ChangeAmount, IsZeroBetweenIdenticalFrames)
{
  SyntheticFrame one{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame other{ FRAME_WIDTH, FRAME_HEIGHT, 10u };
  one.FillRamp(0u, 31u);
  other.FillRamp(0u, 31u);

  auto const change{ Held(ChangeBetween(one.BusView(), other.BusView())) };
  EXPECT_DOUBLE_EQ(change.changed_ratio, 0.0);
  EXPECT_DOUBLE_EQ(change.mean_absolute_difference, 0.0);
}

TEST(ChangeAmount, IsWholeBetweenInvertedFrames)
{
  SyntheticFrame black{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame white{ FRAME_WIDTH, FRAME_HEIGHT };
  black.Fill(RGB565_BLACK);
  white.Fill(RGB565_WHITE);

  auto const change{ Held(ChangeBetween(black.View(), white.View())) };
  EXPECT_DOUBLE_EQ(change.changed_ratio, 1.0);
  // 5/6/5 white greys to 250 of 255, so the mean difference stops short of one.
  EXPECT_GT(change.mean_absolute_difference, 0.95);
}

TEST(ChangeAmount, CountsOnlyThePixelsAMovedRectangleTouches)
{
  SyntheticFrame before{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame after{ FRAME_WIDTH, FRAME_HEIGHT };
  before.Fill(RGB565_BLACK);
  after.Fill(RGB565_BLACK);
  before.FillRectangle(PATCH, RGB565_WHITE);
  after.FillRectangle(MOVED, RGB565_WHITE);

  auto const touched{ 2.0 * PATCH.width * PATCH.height };
  auto const change{ Held(ChangeBetween(before.View(), after.View())) };
  EXPECT_DOUBLE_EQ(change.changed_ratio,
                   touched / (double{ FRAME_WIDTH } * FRAME_HEIGHT));
  EXPECT_GT(change.mean_absolute_difference, 0.0);
}

TEST(ChangeAmount, SeesNothingInARegionTheChangeMissed)
{
  SyntheticFrame before{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame after{ FRAME_WIDTH, FRAME_HEIGHT };
  before.Fill(RGB565_BLACK);
  after.Fill(RGB565_BLACK);
  after.FillRectangle(MOVED, RGB565_WHITE);

  EXPECT_DOUBLE_EQ(
    Held(ChangeBetween(before.View(), after.View(), PATCH)).changed_ratio, 0.0);
  EXPECT_DOUBLE_EQ(
    Held(ChangeBetween(before.View(), after.View(), MOVED)).changed_ratio, 1.0);
}

TEST(ChangeAmount, ToleranceHoldsBackASmallDifference)
{
  SyntheticFrame before{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame after{ FRAME_WIDTH, FRAME_HEIGHT };
  before.Fill(SyntheticFrame::GreyLevel(16u));
  after.Fill(SyntheticFrame::GreyLevel(17u));

  EXPECT_DOUBLE_EQ(
    Held(ChangeBetween(before.View(), after.View())).changed_ratio, 1.0);
  EXPECT_DOUBLE_EQ(
    Held(ChangeBetween(before.View(), after.View(), 8u)).changed_ratio, 0.0);
}

TEST(ChangeAmount, RefusesFramesOfDifferentGeometry)
{
  SyntheticFrame small{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame large{ FRAME_WIDTH * 2u, FRAME_HEIGHT };
  small.Fill(RGB565_BLACK);
  large.Fill(RGB565_BLACK);
  EXPECT_FALSE(ChangeBetween(small.View(), large.View()).has_value());
}
