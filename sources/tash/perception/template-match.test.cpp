#include "tash/perception/template-match.hpp"

#include "_synthetic-frame.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace tash::perception;
  using namespace tash::perception::testing;

  constexpr std::uint32_t FRAME_WIDTH{ 64u };
  constexpr std::uint32_t FRAME_HEIGHT{ 64u };
  constexpr std::uint32_t PATCH_SIDE{ 8u };
  constexpr std::uint32_t PATCH_X{ 20u };
  constexpr std::uint32_t PATCH_Y{ 12u };
  constexpr double        EXACT_SCORE{ 0.99 };

  // A pattern needs variance or normalised cross-correlation has nothing to
  // normalise by; a diagonal ramp gives it some in both directions.
  auto Diagonal(SyntheticFrame& frame, std::uint32_t at_x, std::uint32_t at_y)
    -> void
  {
    for (std::uint32_t y{ 0 }; y < PATCH_SIDE; ++y)
      for (std::uint32_t x{ 0 }; x < PATCH_SIDE; ++x)
        frame.Set(at_x + x, at_y + y, SyntheticFrame::GreyLevel((x + y) * 2u));
  }
}

TEST(TemplateMatch, FindsAPastedPatchWhereItWasPasted)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT, 14u };
  frame.Fill(RGB565_BLACK);
  Diagonal(frame, PATCH_X, PATCH_Y);

  SyntheticFrame pattern{ PATCH_SIDE, PATCH_SIDE };
  Diagonal(pattern, 0u, 0u);

  auto const found{ Held(BestMatch(frame.View(), pattern.View())) };
  EXPECT_GT(found.score, EXACT_SCORE);
  EXPECT_EQ(found.x, PATCH_X);
  EXPECT_EQ(found.y, PATCH_Y);
}

TEST(TemplateMatch, ReportsARegionSearchInTheFramesOwnPixels)
{
  constexpr Region WITHIN{ 16u, 8u, 32u, 32u };
  SyntheticFrame   frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_BLACK);
  Diagonal(frame, PATCH_X, PATCH_Y);

  SyntheticFrame pattern{ PATCH_SIDE, PATCH_SIDE };
  Diagonal(pattern, 0u, 0u);

  auto const found{ Held(BestMatch(frame.BusView(), pattern.View(), WITHIN)) };
  EXPECT_GT(found.score, EXACT_SCORE);
  EXPECT_EQ(found.x, PATCH_X);
  EXPECT_EQ(found.y, PATCH_Y);
}

TEST(TemplateMatch, ScoresAnAbsentPatternLower)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.FillRamp(0u, 31u);

  SyntheticFrame pattern{ PATCH_SIDE, PATCH_SIDE };
  Diagonal(pattern, 0u, 0u);

  EXPECT_LT(Held(BestMatch(frame.View(), pattern.View())).score, EXACT_SCORE);
}

TEST(TemplateMatch, RefusesAPatternLargerThanTheFrame)
{
  SyntheticFrame frame{ PATCH_SIDE, PATCH_SIDE };
  SyntheticFrame pattern{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_BLACK);
  pattern.Fill(RGB565_BLACK);
  EXPECT_FALSE(BestMatch(frame.View(), pattern.View()).has_value());
}
