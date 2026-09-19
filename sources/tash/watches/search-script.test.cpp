#include "tash/watches/search-script.hpp"

#include "tash/libretro/libretro.h"

#include <gtest/gtest.h>

namespace
{
  using namespace tash::watches;
}

TEST(SearchScript, ReadsTheStepsOfAHunt)
{
  auto const steps{ StepsFrom(
    "run 600; snapshot; run 60; changed; run 300; increased; list 5") };
  ASSERT_TRUE(steps.has_value()) << (steps ? "" : steps.error());
  ASSERT_EQ(steps->size(), 7u);
  EXPECT_EQ((*steps)[0].kind, StepKind::RUN);
  EXPECT_EQ((*steps)[0].frames, 600u);
  EXPECT_EQ((*steps)[1].kind, StepKind::SNAPSHOT);
  EXPECT_EQ((*steps)[3].kind, StepKind::NARROW);
  EXPECT_EQ((*steps)[3].how, Comparison::CHANGED);
  EXPECT_EQ((*steps)[5].how, Comparison::INCREASED);
  EXPECT_EQ((*steps)[6].kind, StepKind::LIST);
  EXPECT_EQ((*steps)[6].limit, 5u);
}

TEST(SearchScript, HoldAndReleaseNameJoypadButtons)
{
  auto const steps{ StepsFrom("hold start,a; run 4; release") };
  ASSERT_TRUE(steps.has_value()) << (steps ? "" : steps.error());
  ASSERT_EQ(steps->size(), 3u);
  EXPECT_EQ((*steps)[0].kind, StepKind::HOLD);
  EXPECT_EQ((*steps)[0].pad, (1u << RETRO_DEVICE_ID_JOYPAD_START)
                               | (1u << RETRO_DEVICE_ID_JOYPAD_A));
  EXPECT_EQ((*steps)[2].kind, StepKind::RELEASE);
  EXPECT_FALSE(PadOf("triangle").has_value());
}

TEST(SearchScript, ValueCarriesTheNumberToLookFor)
{
  auto const steps{ StepsFrom("snapshot; value -42") };
  ASSERT_TRUE(steps.has_value()) << (steps ? "" : steps.error());
  ASSERT_EQ(steps->size(), 2u);
  EXPECT_EQ((*steps)[1].how, Comparison::VALUE);
  EXPECT_EQ((*steps)[1].against, -42);
}

TEST(SearchScript, ListTakesACountOrNothing)
{
  auto const bare{ StepsFrom("list") };
  ASSERT_TRUE(bare.has_value());
  EXPECT_EQ(bare->front().limit, DEFAULT_LIST_LIMIT);
}

TEST(SearchScript, AStepThatIsNotAStepSaysWhatIs)
{
  auto const wrong{ StepsFrom("run 600; ponder") };
  ASSERT_FALSE(wrong.has_value());
  EXPECT_NE(wrong.error().find("snapshot"), std::string::npos);

  EXPECT_FALSE(StepsFrom("run").has_value());
  EXPECT_FALSE(StepsFrom("run twice").has_value());
  EXPECT_FALSE(StepsFrom("changed 3").has_value());
  EXPECT_FALSE(StepsFrom("value").has_value());
  EXPECT_FALSE(StepsFrom("   ").has_value());
}

TEST(SearchScript, BlankStepsBetweenSemicolonsAreNotSteps)
{
  auto const steps{ StepsFrom(" snapshot ;; changed ; ") };
  ASSERT_TRUE(steps.has_value()) << (steps ? "" : steps.error());
  EXPECT_EQ(steps->size(), 2u);
}
