#include "tash/tape/anchor-check.hpp"

#include "tash/perception/frame-hash.hpp"
#include "tash/perception/perceptual-hash.hpp"
#include "tash/tape/_test-frame.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

namespace tash::tape
{
  namespace
  {
    constexpr std::uint32_t WIDTH{ 160 };
    constexpr std::uint32_t HEIGHT{ 120 };
    constexpr std::uint32_t PADDING_BYTES{ 8 };
    constexpr perception::Region CORNER{ 8u, 8u, 32u, 24u };

    class OneWatch : public WatchSource
    {
    public:
      explicit OneWatch(std::int64_t value) : _value{ value } { }

      [[nodiscard]] auto Value(std::string_view name) const
        -> std::optional<std::int64_t> override
      {
        if (name != "score")
          return std::nullopt;
        return _value;
      }

    private:
      std::int64_t _value;
    };

    [[nodiscard]] auto Painted(std::uint32_t step) -> testing::TestFrame
    {
      testing::TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
      frame.Paint(step);
      return frame;
    }

    [[nodiscard]] auto Ready(Anchor const& predicate,
                             std::filesystem::path const& tape)
      -> std::optional<AnchorCheck>
    {
      auto made{ AnchorCheck::For(predicate, tape) };
      EXPECT_TRUE(made.has_value()) << (made ? "" : made.error());
      if (!made)
        return std::nullopt;
      return std::move(*made);
    }
  }

  TEST(AnchorHolds, AnExactHashIsTheFrameItWasTakenFrom)
  {
    testing::TestFrame const frame{ Painted(0u) };
    testing::TestFrame const other{ Painted(3u) };
    auto const digest{ perception::ExactHash(frame.View()) };
    ASSERT_TRUE(digest.has_value()) << (digest ? "" : digest.error());

    Anchor predicate{ };
    predicate.kind = AnchorKind::EXACT_HASH;
    predicate.hash = HashText(*digest);

    auto const check{ Ready(predicate, "tape.yaml") };
    ASSERT_TRUE(check.has_value());
    EXPECT_EQ(check->Holds(frame.View(), nullptr), true);
    EXPECT_EQ(check->Holds(other.View(), nullptr), false);
    EXPECT_FALSE(check->Immediate());
  }

  TEST(AnchorHolds, ARegionIsHashedAndNotTheWholeFrame)
  {
    testing::TestFrame frame{ Painted(0u) };
    auto const digest{ perception::ExactHash(frame.View(), CORNER) };
    ASSERT_TRUE(digest.has_value()) << (digest ? "" : digest.error());

    Anchor predicate{ };
    predicate.kind   = AnchorKind::EXACT_HASH;
    predicate.hash   = HashText(*digest);
    predicate.region = AnchorRegion{ CORNER.x, CORNER.y, CORNER.width,
                                     CORNER.height };

    auto const check{ Ready(predicate, "tape.yaml") };
    ASSERT_TRUE(check.has_value());
    EXPECT_EQ(check->Holds(frame.View(), nullptr), true);

    frame.Set(WIDTH - 1u, HEIGHT - 1u, 0u);
    EXPECT_EQ(check->Holds(frame.View(), nullptr), true);
  }

  TEST(AnchorHolds, APerceptualHashHoldsWithinItsDistance)
  {
    testing::TestFrame const frame{ Painted(0u) };
    auto const digest{ perception::PerceptualHash(frame.View()) };
    ASSERT_TRUE(digest.has_value()) << (digest ? "" : digest.error());

    Anchor predicate{ };
    predicate.kind         = AnchorKind::PERCEPTUAL_HASH;
    predicate.hash         = HashText(*digest);
    predicate.max_distance = 0u;

    auto const check{ Ready(predicate, "tape.yaml") };
    ASSERT_TRUE(check.has_value());
    EXPECT_EQ(check->Holds(frame.View(), nullptr), true);
  }

  TEST(AnchorHolds, ATemplateIsFoundInTheFrameItWasCroppedFrom)
  {
    auto const scratch{ utilities::ScratchAreaOf("tape-anchor-image") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

    testing::TestFrame const frame{ Painted(0u) };
    std::filesystem::path const tape{ scratch->File("demo.yaml") };
    auto const written{ WriteAnchorImage(frame.View(), CORNER, tape, "logo") };
    ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
    EXPECT_EQ(*written, "demo.anchors/logo.png");
    EXPECT_TRUE(std::filesystem::exists(AnchorImagePath(tape, "logo")));

    Anchor predicate{ };
    predicate.kind          = AnchorKind::TEMPLATE_IMAGE;
    predicate.image         = *written;
    predicate.minimum_score = 0.99;

    auto const anywhere{ Ready(predicate, tape) };
    ASSERT_TRUE(anywhere.has_value());
    EXPECT_EQ(anywhere->Holds(frame.View(), nullptr), true);

    // The painted band repeats, so the crop is somewhere in every frame; the
    // region is what says where it has to be.
    predicate.region = AnchorRegion{ CORNER.x, CORNER.y, CORNER.width,
                                     CORNER.height };
    auto const here{ Ready(predicate, tape) };
    ASSERT_TRUE(here.has_value());
    EXPECT_EQ(here->Holds(frame.View(), nullptr), true);
    EXPECT_EQ(here->Holds(Painted(7u).View(), nullptr), false);
  }

  TEST(AnchorHolds, AnImageNobodyWroteIsRefusedWhenTheTapeIsRead)
  {
    Anchor predicate{ };
    predicate.kind  = AnchorKind::TEMPLATE_IMAGE;
    predicate.image = "demo.anchors/missing.png";

    auto const check{ AnchorCheck::For(predicate, "/nowhere/demo.yaml") };
    ASSERT_FALSE(check.has_value());
    EXPECT_NE(check.error().find("missing.png"), std::string::npos)
      << check.error();
  }

  TEST(AnchorHolds, AWatchAnchorAsksWhoeverSamplesTheWatch)
  {
    Anchor predicate{ };
    predicate.kind       = AnchorKind::WATCH;
    predicate.watch      = "score";
    predicate.comparison = Comparison::GREATER_OR_EQUAL;
    predicate.value      = 100;

    auto const check{ Ready(predicate, "tape.yaml") };
    ASSERT_TRUE(check.has_value());

    testing::TestFrame const frame{ Painted(0u) };
    OneWatch const low{ 99 };
    OneWatch const high{ 100 };
    EXPECT_EQ(check->Holds(frame.View(), &low), false);
    EXPECT_EQ(check->Holds(frame.View(), &high), true);

    auto const nobody{ check->Holds(frame.View(), nullptr) };
    ASSERT_FALSE(nobody.has_value());
    EXPECT_NE(nobody.error().find("score"), std::string::npos)
      << nobody.error();
  }

  TEST(AnchorHolds, NoAnchorHoldsStraightAway)
  {
    auto const check{ Ready(Anchor{ }, "tape.yaml") };
    ASSERT_TRUE(check.has_value());
    EXPECT_TRUE(check->Immediate());
    EXPECT_EQ(check->Holds(Painted(0u).View(), nullptr), true);
  }
}
