#include "tash/recorder/_frame-queue.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <thread>
#include <vector>

namespace tash::recorder::detail::frame_queue
{
  namespace
  {
    inline constexpr std::size_t PUSHES{ 100 };

    // Slow enough that a tight pushing loop fills a depth of one long before
    // the consumer has taken half of what it sent.
    inline constexpr std::chrono::milliseconds SLOW{ 1 };

    [[nodiscard]] auto Frame(std::int64_t at) -> Work
    {
      return VideoWork{ std::vector<std::uint8_t>(4u, std::uint8_t{ 0 }), 1u,
                        2u, at };
    }

    [[nodiscard]] auto Taking(FrameQueue& queue) -> std::future<std::size_t>
    {
      return std::async(std::launch::async, [&queue] {
        std::size_t taken{ 0 };
        while (queue.Pop())
        {
          ++taken;
          std::this_thread::sleep_for(SLOW);
        }
        return taken;
      });
    }

    [[nodiscard]] auto Refused(FrameQueue& queue) -> std::size_t
    {
      std::size_t refused{ 0 };
      for (std::size_t which{ 0 }; which != PUSHES; ++which)
        if (!queue.Push(Frame(static_cast<std::int64_t>(which))))
          ++refused;
      queue.Finish();
      return refused;
    }
  }

  TEST(RecorderFrameQueue, AFullQueueRefusesTheNewest)
  {
    FrameQueue queue{ 2u };

    EXPECT_TRUE(queue.Push(Frame(0)));
    EXPECT_TRUE(queue.Push(Frame(1)));
    EXPECT_FALSE(queue.Push(Frame(2)));
    EXPECT_FALSE(queue.Push(Frame(3)));

    EXPECT_EQ(queue.Size(), 2u);
  }

  TEST(RecorderFrameQueue, WhatWasPushedComesBackInOrder)
  {
    FrameQueue queue{ 4u };
    EXPECT_TRUE(queue.Push(Frame(10)));
    EXPECT_TRUE(queue.Push(Frame(20)));
    queue.Finish();

    auto const first{ queue.Pop() };
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(std::get<VideoWork>(*first).at, 10);

    auto const second{ queue.Pop() };
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(std::get<VideoWork>(*second).at, 20);

    EXPECT_FALSE(queue.Pop().has_value());
  }

  TEST(RecorderFrameQueue, NothingIsTakenAfterTheQueueIsFinished)
  {
    FrameQueue queue{ 4u };
    queue.Finish();
    EXPECT_FALSE(queue.Push(Frame(0)));
    EXPECT_FALSE(queue.Pop().has_value());
  }

  TEST(RecorderFrameQueue, AWaitingQueueLosesNothingToASlowConsumer)
  {
    FrameQueue queue{ 1u, WhenFull::WAIT };
    std::future<std::size_t> taken{ Taking(queue) };

    EXPECT_EQ(Refused(queue), 0u);
    EXPECT_EQ(taken.get(), PUSHES);
  }

  TEST(RecorderFrameQueue, ADroppingQueueRefusesWhatTheSameConsumerMisses)
  {
    FrameQueue queue{ 1u, WhenFull::DROP };
    std::future<std::size_t> taken{ Taking(queue) };

    std::size_t const refused{ Refused(queue) };
    EXPECT_GT(refused, 0u);
    EXPECT_EQ(taken.get() + refused, PUSHES);
  }

  TEST(RecorderFrameQueue, FinishingLetsGoOfAPusherThatIsWaiting)
  {
    FrameQueue queue{ 1u, WhenFull::WAIT };
    ASSERT_TRUE(queue.Push(Frame(0)));

    std::future<bool> waiting{ std::async(std::launch::async,
                                          [&queue] {
                                            return queue.Push(Frame(1));
                                          }) };
    ASSERT_EQ(waiting.wait_for(SLOW), std::future_status::timeout);

    queue.Finish();
    EXPECT_FALSE(waiting.get());
    EXPECT_EQ(queue.Size(), 1u);
  }
}
