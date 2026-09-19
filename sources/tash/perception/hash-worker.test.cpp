#include "tash/perception/hash-worker.hpp"

#include "_synthetic-frame.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace
{
  using namespace tash::perception;
  using namespace tash::perception::testing;

  constexpr std::uint32_t FRAME_WIDTH{ 40u };
  constexpr std::uint32_t FRAME_HEIGHT{ 32u };
  constexpr std::uint64_t SUBMITTED{ 64u };
  constexpr std::size_t   SMALL_QUEUE{ 2u };

  [[nodiscard]] auto FrameNumbered(std::uint64_t number,
                                   std::uint32_t padding = 0u) -> SyntheticFrame
  {
    SyntheticFrame frame{ FRAME_WIDTH, FRAME_HEIGHT, padding };
    frame.FillRamp(0u, 31u);
    frame.Set(static_cast<std::uint32_t>(number % FRAME_WIDTH), 0u,
              RGB565_WHITE);
    return frame;
  }
}

TEST(HashWorker, ReturnsEveryResultInSubmissionOrder)
{
  HashWorker worker{ SMALL_QUEUE };
  for (std::uint64_t number{ 0 }; number < SUBMITTED; ++number)
    EXPECT_TRUE(
      worker.Submit(FrameNumbered(number).BusView(number)).has_value());
  worker.Drain();

  EXPECT_EQ(worker.Pending(), 0u);
  ASSERT_EQ(worker.Ready(), SUBMITTED);
  for (std::uint64_t number{ 0 }; number < SUBMITTED; ++number)
  {
    auto const hashed{ worker.TryTake() };
    ASSERT_TRUE(hashed.has_value());
    EXPECT_EQ(hashed->frame, number);
    EXPECT_EQ(Held(hashed->hashes),
              Held(HashesOf(FrameNumbered(number).View())));
  }
  EXPECT_FALSE(worker.TryTake().has_value());
}

TEST(HashWorker, HashesThePictureRatherThanTheBuffer)
{
  HashWorker worker{};
  EXPECT_TRUE(worker.Submit(7u, FrameNumbered(7u).View()).has_value());
  EXPECT_TRUE(worker.Submit(7u, FrameNumbered(7u, 18u).View()).has_value());
  worker.Drain();

  auto const tight{ worker.Take() };
  auto const padded{ worker.Take() };
  ASSERT_TRUE(tight.has_value());
  ASSERT_TRUE(padded.has_value());
  EXPECT_EQ(Held(tight->hashes), Held(padded->hashes));
}

TEST(HashWorker, TakeGivesUpWhenNothingIsComing)
{
  HashWorker worker{};
  EXPECT_FALSE(worker.Take().has_value());
}

TEST(HashWorker, HoldsAtMostItsCapacityOfUnhashedFrames)
{
  HashWorker worker{ SMALL_QUEUE };
  EXPECT_EQ(worker.Capacity(), SMALL_QUEUE);
  for (std::uint64_t number{ 0 }; number < SUBMITTED; ++number)
  {
    EXPECT_TRUE(
      worker.Submit(number, FrameNumbered(number).View()).has_value());
    EXPECT_LE(worker.Pending(), SMALL_QUEUE + 1u);
  }
  worker.Drain();
  EXPECT_EQ(worker.Ready(), SUBMITTED);
}

TEST(HashWorker, DrainsWhatItWasGivenBeforeItGoesAway)
{
  std::vector<HashedFrame> taken;
  {
    HashWorker worker{ SMALL_QUEUE };
    for (std::uint64_t number{ 0 }; number < SUBMITTED; ++number)
      EXPECT_TRUE(
        worker.Submit(number, FrameNumbered(number).View()).has_value());
    worker.Drain();
    while (auto hashed = worker.TryTake())
      taken.push_back(std::move(*hashed));
  }
  EXPECT_EQ(taken.size(), SUBMITTED);
}

TEST(HashWorker, RefusesAViewThatDoesNotDescribeItsPixels)
{
  HashWorker worker{};
  EXPECT_FALSE(worker.Submit(0u, Rgb565View{}).has_value());
  EXPECT_FALSE(worker.TrySubmit(0u, Rgb565View{}).has_value());
}

TEST(HashWorker, MeasuresTheChangeFromTheFrameSubmittedBefore)
{
  HashWorker worker{};
  SyntheticFrame still{ FRAME_WIDTH, FRAME_HEIGHT };
  still.FillRamp(0u, 31u);
  SyntheticFrame moved{ FRAME_WIDTH, FRAME_HEIGHT };
  moved.FillRamp(31u, 0u);

  for (auto const* frame : { &still, &still, &moved })
    EXPECT_TRUE(worker.Submit(0u, frame->View()).has_value());
  worker.Drain();

  auto const first{ worker.TryTake() };
  auto const repeated{ worker.TryTake() };
  auto const changed{ worker.TryTake() };
  ASSERT_TRUE(first && repeated && changed);
  EXPECT_EQ(first->change.changed_ratio, 0.0);
  EXPECT_EQ(repeated->change.changed_ratio, 0.0);
  EXPECT_GT(changed->change.changed_ratio, 0.0);
  EXPECT_GT(changed->change.mean_absolute_difference, 0.0);
}
