#include "tash/perception/region-stability.hpp"

#include "_synthetic-frame.hpp"
#include "tash/perception/frame-hash.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace tash::perception;
  using namespace tash::perception::testing;

  constexpr std::uint32_t FRAME_WIDTH{ 64u };
  constexpr std::uint32_t FRAME_HEIGHT{ 64u };
  constexpr Region        STILL{ 0u, 0u, 16u, 16u };
  constexpr Region        BUSY{ 32u, 32u, 16u, 16u };
}

TEST(RegionStability, StartsAtZeroAndRisesOncePerUnchangedFrame)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_BLACK);
  RegionStability stability{ { STILL, BUSY } };

  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_EQ(Held(stability.StableFrames(0u)), 0u);
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_EQ(Held(stability.StableFrames(0u)), 1u);
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_EQ(Held(stability.StableFrames(0u)), 2u);
}

TEST(RegionStability, ResetsOnlyTheRegionThatMoved)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_BLACK);
  RegionStability stability{ { STILL, BUSY } };
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());

  frame.FillRectangle(BUSY, RGB565_WHITE);
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_EQ(Held(stability.StableFrames(0u)), 3u);
  EXPECT_EQ(Held(stability.StableFrames(1u)), 0u);
}

TEST(RegionStability, KeepsTheHashItLastSaw)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT, 12u };
  frame.FillRamp(0u, 31u);
  RegionStability stability{ { STILL } };
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());

  EXPECT_EQ(stability.Count(), 1u);
  EXPECT_EQ(Held(stability.RegionAt(0u)), STILL);
  EXPECT_EQ(Held(stability.LastHash(0u)), Held(ExactHash(frame.View(), STILL)));
}

TEST(RegionStability, ForgetsEverythingOnReset)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_BLACK);
  RegionStability stability{ { STILL } };
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  ASSERT_TRUE(stability.Seeded());

  stability.Reset();
  EXPECT_FALSE(stability.Seeded());
  EXPECT_EQ(Held(stability.StableFrames(0u)), 0u);
  EXPECT_TRUE(stability.Observe(frame.View()).has_value());
  EXPECT_EQ(Held(stability.StableFrames(0u)), 0u);
}
