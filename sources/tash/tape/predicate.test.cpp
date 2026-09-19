#include "tash/tape/predicate.hpp"

#include "tash/perception/frame-hash.hpp"
#include "tash/perception/perceptual-hash.hpp"
#include "tash/tape/_test-frame.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace tash::tape
{
  using utilities::Result;

  namespace
  {
    constexpr std::uint32_t WIDTH{ 160 };
    constexpr std::uint32_t HEIGHT{ 120 };
    constexpr std::uint32_t PADDING_BYTES{ 8 };
    constexpr std::uint64_t AT{ 42 };

    // Five bits widen by a shift, so the brightest red a frame holds is 248.
    constexpr std::uint16_t RGB565_RED{ 0xF800 };
    constexpr std::uint16_t RGB565_BLUE{ 0x001F };
    constexpr std::string_view RED_CROP{ "0,0,80,120" };

    class OneWatch : public WatchSource
    {
    public:
      explicit OneWatch(std::int64_t value) : _value{ value } { }

      [[nodiscard]] auto Value(std::string_view name) const
        -> std::optional<std::int64_t> override
      {
        if (name != "money")
          return std::nullopt;
        return _value;
      }

    private:
      std::int64_t _value;
    };

    [[nodiscard]] auto Painted() -> testing::TestFrame
    {
      testing::TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
      frame.Paint(0u);
      return frame;
    }

    // The left half red, the right half blue, so a count over a crop is
    // arithmetic rather than a measurement.
    [[nodiscard]] auto Halved() -> testing::TestFrame
    {
      testing::TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
      for (std::uint32_t y{ 0 }; y < HEIGHT; ++y)
        for (std::uint32_t x{ 0 }; x < WIDTH; ++x)
          frame.Set(x, y, x < WIDTH / 2 ? RGB565_RED : RGB565_BLUE);
      return frame;
    }

    [[nodiscard]] auto AnsweredOn(testing::TestFrame const& frame,
                                  std::string_view text) -> Result<Judged>
    {
      Result<AnchorCheck> const check{ CheckFrom(text) };
      if (!check)
        return utilities::Forwarded(check);
      return JudgedOn(*check, frame.View(), nullptr, AT);
    }

    [[nodiscard]] auto Answered(std::string_view text,
                                WatchSource const* watches) -> Result<Judged>
    {
      testing::TestFrame const frame{ Painted() };
      Result<AnchorCheck> const check{ CheckFrom(text) };
      if (!check)
        return utilities::Forwarded(check);
      return JudgedOn(*check, frame.View(), watches, AT);
    }
  }

  TEST(Predicate, AWatchPredicateReadsAsTheWordsThatWroteIt)
  {
    OneWatch const watches{ 2500 };
    auto const seen{ Answered("watch money greater 2000", &watches) };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
    EXPECT_TRUE(seen->passed);
    EXPECT_EQ(seen->wording,
              std::format("watch money greater 2000 holds at frame {}", AT));

    auto const under{ Answered("watch money greater 9000", &watches) };
    ASSERT_TRUE(under.has_value()) << (under ? "" : under.error());
    EXPECT_FALSE(under->passed);
    EXPECT_EQ(under->wording,
              std::format("watch money greater 9000 does not hold at frame {}",
                          AT));
  }

  TEST(Predicate, AnExactHashPredicateIsTheFrameItWasTakenFrom)
  {
    testing::TestFrame const frame{ Painted() };
    auto const digest{ perception::ExactHash(frame.View()) };
    ASSERT_TRUE(digest.has_value()) << (digest ? "" : digest.error());

    auto const seen{ Answered(std::format("exact_hash {}", HashText(*digest)),
                              nullptr) };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
    EXPECT_TRUE(seen->passed);

    auto const other{ Answered("exact_hash 0000000000000000", nullptr) };
    ASSERT_TRUE(other.has_value()) << (other ? "" : other.error());
    EXPECT_FALSE(other->passed);
  }

  TEST(Predicate, ADifferenceHashPredicateCarriesTheDistanceItWasGiven)
  {
    testing::TestFrame const frame{ Painted() };
    auto const digest{ perception::DifferenceHash(frame.View()) };
    ASSERT_TRUE(digest.has_value()) << (digest ? "" : digest.error());

    auto const seen{ Answered(
      std::format("difference_hash {} within 0", HashText(*digest)),
      nullptr) };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
    EXPECT_TRUE(seen->passed);
    EXPECT_NE(seen->wording.find("within 0"), std::string::npos)
      << seen->wording;
  }

  TEST(Predicate, AColourPredicateCountsOverTheCropItNames)
  {
    testing::TestFrame const frame{ Halved() };
    std::uint64_t const red{ std::uint64_t{ WIDTH } / 2 * HEIGHT };

    auto const seen{ AnsweredOn(
      frame, std::format("colour 248,0,0 within 0 over {} at least {}",
                         RED_CROP, red)) };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
    EXPECT_TRUE(seen->passed);
    EXPECT_EQ(seen->wording,
              std::format("colour 248,0,0 within 0 over {} at least {} holds"
                          " at frame {}", RED_CROP, red, AT));

    auto const more{ AnsweredOn(
      frame, std::format("colour 248,0,0 within 0 over {} at least {}",
                         RED_CROP, red + 1)) };
    ASSERT_TRUE(more.has_value()) << (more ? "" : more.error());
    EXPECT_FALSE(more->passed);

    auto const blue{ AnsweredOn(
      frame, std::format("colour 0,0,248 within 0 over {} at least 1",
                         RED_CROP)) };
    ASSERT_TRUE(blue.has_value()) << (blue ? "" : blue.error());
    EXPECT_FALSE(blue->passed);
  }

  TEST(Predicate, AColourPredicateMissingItsWordsIsRefusedByTheForm)
  {
    Result<AnchorCheck> const half{ CheckFrom("colour 248,0,0 within 0") };
    ASSERT_FALSE(half.has_value());
    EXPECT_NE(half.error().find("<r,g,b> within <n> over <x,y,w,h> at least"
                                " <count>"), std::string::npos)
      << half.error();

    Result<AnchorCheck> const channels{
      CheckFrom("colour 248,0 within 0 over 0,0,8,8 at least 1") };
    ASSERT_FALSE(channels.has_value());
    EXPECT_NE(channels.error().find("a colour is r,g,b"), std::string::npos)
      << channels.error();

    Result<AnchorCheck> const short_channels{
      CheckFrom("colour 1,2 within 0 over 0,0,8,8 at least 1") };
    ASSERT_FALSE(short_channels.has_value());
    EXPECT_NE(short_channels.error().find("a colour is r,g,b"),
              std::string::npos) << short_channels.error();

    Result<AnchorCheck> const trailing{
      CheckFrom("colour 1,2, within 0 over 0,0,8,8 at least 1") };
    ASSERT_FALSE(trailing.has_value());
    EXPECT_NE(trailing.error().find("a colour is r,g,b"),
              std::string::npos) << trailing.error();

    Result<AnchorCheck> const wide{
      CheckFrom("colour 248,0,300 within 0 over 0,0,8,8 at least 1") };
    ASSERT_FALSE(wide.has_value());
    EXPECT_NE(wide.error().find("not a colour"), std::string::npos)
      << wide.error();

    Result<AnchorCheck> const crop{
      CheckFrom("colour 248,0,0 within 0 over 0,0,8 at least 1") };
    ASSERT_FALSE(crop.has_value());
    EXPECT_NE(crop.error().find("a region is x,y,width,height"),
              std::string::npos) << crop.error();
  }

  TEST(Predicate, NoneHoldsOnAnyFrame)
  {
    auto const seen{ Answered("none", nullptr) };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
    EXPECT_TRUE(seen->passed);
    EXPECT_EQ(seen->wording, std::format("none holds at frame {}", AT));
  }

  TEST(Predicate, AStringTheGrammarDoesNotKnowIsRefusedByName)
  {
    Result<AnchorCheck> const nonsense{ CheckFrom("money > 2000") };
    ASSERT_FALSE(nonsense.has_value());
    EXPECT_EQ(nonsense.error(), "tape: 'money' is not an anchor kind");

    Result<AnchorCheck> const empty{ CheckFrom("") };
    ASSERT_FALSE(empty.has_value());
    EXPECT_NE(empty.error().find("says nothing to wait for"),
              std::string::npos) << empty.error();

    Result<AnchorCheck> const short_form{ CheckFrom("watch money greater") };
    ASSERT_FALSE(short_form.has_value());
    EXPECT_NE(short_form.error().find("<name> <comparison> <value>"),
              std::string::npos) << short_form.error();

    Result<AnchorCheck> const how{ CheckFrom("watch money over 2000") };
    ASSERT_FALSE(how.has_value());
    EXPECT_EQ(how.error(), "tape: 'over' is not a comparison");

    Result<AnchorCheck> const hash{ CheckFrom("exact_hash zz") };
    ASSERT_FALSE(hash.has_value());
    EXPECT_NE(hash.error().find("is not a hash"), std::string::npos)
      << hash.error();
  }

  TEST(Predicate, AWatchNobodySamplesRefusesRatherThanAnswering)
  {
    auto const nobody{ Answered("watch money greater 2000", nullptr) };
    ASSERT_FALSE(nobody.has_value());
    EXPECT_NE(nobody.error().find("has nobody to ask"), std::string::npos)
      << nobody.error();

    OneWatch const watches{ 10 };
    auto const unknown{ Answered("watch rent greater 1", &watches) };
    ASSERT_FALSE(unknown.has_value());
    EXPECT_NE(unknown.error().find("nobody samples the watch"),
              std::string::npos) << unknown.error();
  }

  TEST(Predicate, AnOrHoldsWhenEitherSideDoesOnTheSameFrame)
  {
    testing::TestFrame const frame{ Halved() };
    std::uint64_t const half{ std::uint64_t{ WIDTH } / 2 * HEIGHT };
    std::string const red{ std::format(
      "colour 248,0,0 within 0 over {} at least {}", RED_CROP, half) };
    std::string const blue{ std::format(
      "colour 0,0,248 within 0 over {} at least 1", RED_CROP) };

    auto const either{ AnsweredOn(frame, std::format("{} or {}", red,
                                                     blue)) };
    ASSERT_TRUE(either.has_value()) << (either ? "" : either.error());
    EXPECT_TRUE(either->passed);
    EXPECT_EQ(either->wording,
              std::format("{} or {} holds at frame {}", red, blue, AT));

    // The same two the other way round: no side is asked first, so the
    // answer and its wording follow the text and nothing else.
    auto const swapped{ AnsweredOn(frame, std::format("{} or {}", blue,
                                                      red)) };
    ASSERT_TRUE(swapped.has_value()) << (swapped ? "" : swapped.error());
    EXPECT_EQ(swapped->passed, either->passed);
    EXPECT_EQ(swapped->wording,
              std::format("{} or {} holds at frame {}", blue, red, AT));

    auto const neither{ AnsweredOn(frame,
                                   std::format("{} or {}", blue, blue)) };
    ASSERT_TRUE(neither.has_value()) << (neither ? "" : neither.error());
    EXPECT_FALSE(neither->passed);
  }

  TEST(Predicate, AnOrOfThreeTermsIsOneAnchorTheWayItWasTyped)
  {
    Result<Anchor> const three{
      AnchorFrom("none or exact_hash 0000000000000000 or watch money"
                 " greater 1") };
    ASSERT_TRUE(three.has_value()) << (three ? "" : three.error());
    EXPECT_EQ(three->kind, AnchorKind::ANY_OF);
    ASSERT_TRUE(three->any_of.has_value());
    EXPECT_EQ(three->any_of->size(), 3u);
    EXPECT_EQ(three->any_of->front().kind, AnchorKind::NONE);
  }

  TEST(Predicate, NotTurnsATermOverAndBindsTighterThanOr)
  {
    OneWatch const watches{ 2500 };
    auto const over{ Answered("not watch money greater 2000", &watches) };
    ASSERT_TRUE(over.has_value()) << (over ? "" : over.error());
    EXPECT_FALSE(over->passed);
    EXPECT_EQ(over->wording,
              std::format("not watch money greater 2000 does not hold at"
                          " frame {}", AT));

    // `not` reaches only the term beside it, so the second side is read on
    // its own and carries the disjunction.
    auto const either{ Answered(
      "not watch money greater 2000 or watch money greater 2000",
      &watches) };
    ASSERT_TRUE(either.has_value()) << (either ? "" : either.error());
    EXPECT_TRUE(either->passed);

    auto const never{ Answered("not none", nullptr) };
    ASSERT_TRUE(never.has_value()) << (never ? "" : never.error());
    EXPECT_FALSE(never->passed);
  }

  TEST(Predicate, ARefusalOnOneSideRefusesTheWholePredicate)
  {
    auto const unread{ Answered("none or watch money greater 1", nullptr) };
    ASSERT_FALSE(unread.has_value());
    EXPECT_NE(unread.error().find("has nobody to ask"), std::string::npos)
      << unread.error();
  }

  TEST(Predicate, AnOrAndANotWithNothingToJoinAreRefusedByName)
  {
    Result<AnchorCheck> const dangling{ CheckFrom("none or") };
    ASSERT_FALSE(dangling.has_value());
    EXPECT_NE(dangling.error().find("has nothing after it"),
              std::string::npos) << dangling.error();

    Result<AnchorCheck> const leading{ CheckFrom("or none") };
    ASSERT_FALSE(leading.has_value());
    EXPECT_EQ(leading.error(), "tape: 'or' is not an anchor kind");

    Result<AnchorCheck> const bare{ CheckFrom("not") };
    ASSERT_FALSE(bare.has_value());
    EXPECT_NE(bare.error().find("negates nothing"), std::string::npos)
      << bare.error();

    Result<AnchorCheck> const joined{ CheckFrom("none none") };
    ASSERT_FALSE(joined.has_value());
    EXPECT_NE(joined.error().find("is not 'or'"), std::string::npos)
      << joined.error();

    Result<AnchorCheck> const typed{ CheckFrom("any_of") };
    ASSERT_FALSE(typed.has_value());
    EXPECT_NE(typed.error().find("<predicate> or <predicate>"),
              std::string::npos) << typed.error();
  }
}
