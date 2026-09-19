#include "tash/perception/colour-count.hpp"

#include "tash/perception/colour.hpp"
#include "tash/perception/region.hpp"

#include "_synthetic-frame.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace tash::perception;
  using namespace tash::perception::testing;

  constexpr std::uint32_t FRAME_WIDTH{ 64u };
  constexpr std::uint32_t FRAME_HEIGHT{ 64u };
  constexpr std::uint64_t FRAME_PIXELS{ std::uint64_t{ FRAME_WIDTH }
                                        * FRAME_HEIGHT };
  constexpr Region        PATCH{ 8u, 8u, 8u, 8u };
  constexpr std::uint64_t PATCH_PIXELS{ std::uint64_t{ PATCH.width }
                                        * PATCH.height };

  constexpr std::uint16_t RGB565_RED{ 0xF800 };
  constexpr std::uint16_t RGB565_BLUE{ 0x001F };

  // Five bits widen by a shift, so the brightest red a frame holds is 248.
  constexpr Colour RED{ 248u, 0u, 0u };
  constexpr Colour BLUE{ 0u, 0u, 248u };
  constexpr Colour PURE_RED{ 255u, 0u, 0u };
  constexpr Colour WHITE{ 255u, 255u, 255u };
  constexpr Region PATCH_AT_ORIGIN{ 0u, 0u, 8u, 8u };
  constexpr std::uint32_t SHORT_OF_PURE{ 7u };
}

TEST(ColourCount, CountsEveryPixelOfAColourThatFillsTheFrame)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_RED);

  EXPECT_EQ(Held(ColourCount(frame.BusView(), RED, 0u)), FRAME_PIXELS);
}

TEST(ColourCount, AnswersNothingForAColourTheFrameDoesNotHold)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_RED);

  EXPECT_EQ(Held(ColourCount(frame.View(), BLUE, 0u)), 0u);
  EXPECT_EQ(Held(ColourCount(frame.View(), PURE_RED, 0u)), 0u);
}

TEST(ColourCount, ToleranceReachesAColourTheFrameRoundedOff)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_RED);

  EXPECT_EQ(Held(ColourCount(frame.View(), PURE_RED, SHORT_OF_PURE)),
            FRAME_PIXELS);
}

TEST(ColourCount, CountsInsideTheRegionAndNotOutsideIt)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT, 10u };
  frame.Fill(RGB565_BLUE);
  frame.FillRectangle(PATCH, RGB565_RED);

  EXPECT_EQ(Held(ColourCount(frame.BusView(), PATCH, RED, 0u)),
            PATCH_PIXELS);
  EXPECT_EQ(Held(ColourCount(frame.BusView(), PATCH, BLUE, 0u)), 0u);
  EXPECT_EQ(Held(ColourCount(frame.View(), RED, 0u)), PATCH_PIXELS);
  EXPECT_EQ(Held(ColourCount(frame.View(), BLUE, 0u)),
            FRAME_PIXELS - PATCH_PIXELS);
}

TEST(ColourCount, RefusesARegionThatLeavesTheFrame)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_RED);

  Region const past{ FRAME_WIDTH - 4u, 0u, 8u, 8u };
  EXPECT_FALSE(ColourCount(frame.View(), past, RED, 0u).has_value());
}

TEST(ColourCount, RefusesAChannelThatIsNotEightBits)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_RED);

  Colour const impossible{ CHANNEL_MAX + 1u, 0u, 0u };
  EXPECT_FALSE(ColourCount(frame.View(), impossible, 0u).has_value());
}

TEST(ColourFrom, ReadsTheThreeChannelsACallerWrites)
{
  EXPECT_EQ(Held(ColourFrom("248,0,0")), RED);
  EXPECT_EQ(Held(ColourFrom("0,0,248")), BLUE);
  EXPECT_EQ(Held(ColourFrom("255,255,255")), WHITE);
}

TEST(ColourFrom, RefusesATextThatIsNotThreeChannels)
{
  EXPECT_FALSE(ColourFrom("").has_value());
  EXPECT_FALSE(ColourFrom("1,2").has_value());
  EXPECT_FALSE(ColourFrom("1,2,").has_value());
  EXPECT_FALSE(ColourFrom(",1,2").has_value());
  EXPECT_FALSE(ColourFrom("1,2,3,4").has_value());
  EXPECT_FALSE(ColourFrom("1,2,x").has_value());
  EXPECT_FALSE(ColourFrom("1,2,3 ").has_value());
}

TEST(ColourFrom, RefusesAChannelPastEightBits)
{
  EXPECT_FALSE(ColourFrom("256,0,0").has_value());
  EXPECT_FALSE(ColourFrom("0,0,1000").has_value());
}

TEST(RegionFrom, ReadsTheFourNumbersACallerWrites)
{
  EXPECT_EQ(Held(RegionFrom("1,2,3,4")), (Region{ 1u, 2u, 3u, 4u }));
  EXPECT_EQ(Held(RegionFrom("0,0,8,8")), PATCH_AT_ORIGIN);
}

TEST(RegionFrom, RefusesATextThatIsNotFourNumbers)
{
  EXPECT_FALSE(RegionFrom("").has_value());
  EXPECT_FALSE(RegionFrom("1,2,3").has_value());
  EXPECT_FALSE(RegionFrom("1,2,3,4,").has_value());
  EXPECT_FALSE(RegionFrom("1,2,3,4,5").has_value());
  EXPECT_FALSE(RegionFrom(",1,2,3").has_value());
  EXPECT_FALSE(RegionFrom("1,2,3,x").has_value());
}
