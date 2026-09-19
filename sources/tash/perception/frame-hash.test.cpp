#include "tash/perception/frame-hash.hpp"

#include "_synthetic-frame.hpp"
#include "tash/perception/hamming-distance.hpp"

#include <gtest/gtest.h>
#include <xxhash.h>

#include <vector>

namespace
{
  using namespace tash::perception;
  using namespace tash::perception::testing;

  constexpr std::uint32_t FRAME_WIDTH{ 72u };
  constexpr std::uint32_t FRAME_HEIGHT{ 64u };
  constexpr std::uint64_t ALL_BITS{ ~std::uint64_t{ 0 } };
  constexpr std::uint32_t HASH_BITS{ 64u };

  [[nodiscard]] auto PackedCopy(Rgb565View const& frame)
    -> std::vector<std::uint8_t>
  {
    std::vector<std::uint8_t> packed;
    for (std::uint32_t y{ 0 }; y < frame.height; ++y)
    {
      auto const row{ frame.Row(y) };
      packed.insert(packed.end(), row.begin(), row.end());
    }
    return packed;
  }

  // The digest session.test.cpp streams, kept here so the two cannot drift.
  [[nodiscard]] auto StreamedDigest(tash::bus::FrameView const& frame)
    -> std::uint64_t
  {
    XXH3_state_t* const state{ XXH3_createState() };
    XXH3_64bits_reset(state);
    std::size_t const row{ std::size_t{ frame.descriptor.width }
                           * tash::bus::BYTES_PER_PIXEL };
    for (std::uint32_t line{ 0 }; line < frame.descriptor.height; ++line)
      XXH3_64bits_update(
        state, frame.pixels.data() + line * frame.descriptor.pitch, row);
    std::uint64_t const digest{ XXH3_64bits_digest(state) };
    XXH3_freeState(state);
    return digest;
  }
}

TEST(ExactHash, IsXxh3OverTheVisiblePixels)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.FillRamp(0u, 31u);
  auto const packed{ PackedCopy(frame.View()) };
  EXPECT_EQ(Held(ExactHash(frame.View())),
            XXH3_64bits(packed.data(), packed.size()));
}

TEST(ExactHash, MatchesTheDigestTheSessionStreamsRowByRow)
{
  SyntheticFrame tight{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame padded{ FRAME_WIDTH, FRAME_HEIGHT, 24u };
  tight.FillRamp(0u, 31u);
  padded.FillRamp(0u, 31u);
  EXPECT_EQ(Held(ExactHash(tight.BusView())), StreamedDigest(tight.BusView()));
  EXPECT_EQ(Held(ExactHash(padded.BusView())),
            StreamedDigest(padded.BusView()));
}

TEST(ExactHash, IgnoresPitchPadding)
{
  SyntheticFrame tight{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame padded{ FRAME_WIDTH, FRAME_HEIGHT, 24u };
  tight.FillRamp(0u, 31u);
  padded.FillRamp(0u, 31u);
  EXPECT_EQ(Held(ExactHash(tight.View())), Held(ExactHash(padded.View())));
  EXPECT_EQ(Held(HashesOf(tight.View())), Held(HashesOf(padded.View())));
}

TEST(ExactHash, SeparatesFlatColours)
{
  SyntheticFrame black{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame white{ FRAME_WIDTH, FRAME_HEIGHT };
  black.Fill(RGB565_BLACK);
  white.Fill(RGB565_WHITE);
  EXPECT_NE(Held(ExactHash(black.View())), Held(ExactHash(white.View())));
}

TEST(ExactHash, RefusesAViewThatDoesNotDescribeItsPixels)
{
  EXPECT_FALSE(ExactHash(Rgb565View{}).has_value());
}

TEST(RegionHash, MatchesAStandaloneFrameOfTheSamePixels)
{
  constexpr Region PATCH{ 16u, 8u, 16u, 16u };
  SyntheticFrame   frame{ FRAME_WIDTH, FRAME_HEIGHT, 6u };
  frame.Fill(RGB565_BLACK);
  frame.FillRectangle(PATCH, RGB565_WHITE);

  SyntheticFrame alone{ PATCH.width, PATCH.height };
  alone.Fill(RGB565_WHITE);
  EXPECT_EQ(Held(ExactHash(frame.View(), PATCH)),
            Held(ExactHash(alone.View())));
  EXPECT_EQ(Held(ExactHash(frame.BusView(), PATCH)),
            Held(ExactHash(alone.View())));
}

TEST(RegionHash, RefusesARegionOffTheFrame)
{
  SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT };
  frame.Fill(RGB565_BLACK);
  EXPECT_FALSE(
    ExactHash(frame.View(), Region{ FRAME_WIDTH, 0u, 8u, 8u }).has_value());
}

TEST(DifferenceHash, IsClearWhenNothingFallsToTheRight)
{
  SyntheticFrame flat{ FRAME_WIDTH, FRAME_HEIGHT };
  flat.Fill(RGB565_WHITE);
  EXPECT_EQ(Held(DifferenceHash(flat.View())), 0u);

  SyntheticFrame rising{ FRAME_WIDTH, FRAME_HEIGHT };
  rising.FillRamp(0u, 31u);
  EXPECT_EQ(Held(DifferenceHash(rising.View())), 0u);
}

TEST(DifferenceHash, IsSetEverywhereOnAFallingRamp)
{
  SyntheticFrame falling{ FRAME_WIDTH, FRAME_HEIGHT };
  falling.FillRamp(31u, 0u);
  EXPECT_EQ(Held(DifferenceHash(falling.View())), ALL_BITS);
}

TEST(DifferenceHash, RidesOutASinglePixelTheExactHashCannot)
{
  SyntheticFrame clean{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame specked{ FRAME_WIDTH, FRAME_HEIGHT };
  clean.FillRamp(0u, 31u);
  specked.FillRamp(0u, 31u);
  specked.Set(FRAME_WIDTH / 2u, FRAME_HEIGHT / 3u, RGB565_WHITE);

  EXPECT_NE(Held(ExactHash(clean.View())), Held(ExactHash(specked.View())));
  EXPECT_EQ(Held(DifferenceHash(clean.View())),
            Held(DifferenceHash(specked.View())));
}

TEST(PerceptualHash, IsClearOnAnEmptyPicture)
{
  SyntheticFrame black{ FRAME_WIDTH, FRAME_HEIGHT };
  black.Fill(RGB565_BLACK);
  EXPECT_EQ(Held(PerceptualHash(black.View())), 0u);
}

TEST(PerceptualHash, HoldsStillForTheSamePictureAndMovesForAnother)
{
  SyntheticFrame rising{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame again{ FRAME_WIDTH, FRAME_HEIGHT, 16u };
  SyntheticFrame blocked{ FRAME_WIDTH, FRAME_HEIGHT };
  rising.FillRamp(0u, 31u);
  again.FillRamp(0u, 31u);
  blocked.FillRamp(0u, 31u);
  blocked.FillRectangle(Region{ 8u, 8u, 16u, 16u }, RGB565_WHITE);

  EXPECT_EQ(Held(PerceptualHash(rising.View())),
            Held(PerceptualHash(again.View())));
  EXPECT_GT(HammingDistance(Held(PerceptualHash(rising.View())),
                            Held(PerceptualHash(blocked.View()))),
            0u);
}

TEST(HammingDistance, CountsTheBitsThatDiffer)
{
  EXPECT_EQ(HammingDistance(ALL_BITS, ALL_BITS), 0u);
  EXPECT_EQ(HammingDistance(0u, ALL_BITS), HASH_BITS);

  SyntheticFrame rising{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame falling{ FRAME_WIDTH, FRAME_HEIGHT };
  rising.FillRamp(0u, 31u);
  falling.FillRamp(31u, 0u);
  EXPECT_EQ(HammingDistance(Held(DifferenceHash(rising.View())),
                            Held(DifferenceHash(falling.View()))),
            HASH_BITS);
}

TEST(FrameHashes, MovingARectangleChangesEveryHash)
{
  SyntheticFrame before{ FRAME_WIDTH, FRAME_HEIGHT };
  SyntheticFrame after{ FRAME_WIDTH, FRAME_HEIGHT };
  before.Fill(RGB565_BLACK);
  after.Fill(RGB565_BLACK);
  before.FillRectangle(Region{ 8u, 8u, 16u, 16u }, RGB565_WHITE);
  after.FillRectangle(Region{ 40u, 8u, 16u, 16u }, RGB565_WHITE);

  auto const one{ Held(HashesOf(before.BusView(1u))) };
  auto const other{ Held(HashesOf(after.BusView(2u))) };
  EXPECT_NE(one.exact, other.exact);
  EXPECT_GT(HammingDistance(one.difference, other.difference), 0u);
  EXPECT_GT(HammingDistance(one.perceptual, other.perceptual), 0u);
}
