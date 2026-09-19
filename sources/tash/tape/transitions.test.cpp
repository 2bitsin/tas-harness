#include "tash/tape/transitions.hpp"

#include "tash/tape/channel.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace tash::tape
{
  namespace
  {
    constexpr unsigned START{ 3 };
    constexpr unsigned RIGHT{ 7 };
  }

  TEST(Transitions, ALineIsAFrameADirectionAndAChannel)
  {
    auto const moves{ TransitionsFrom("0 down p1.start\n2 up p1.start\n") };
    ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
    ASSERT_EQ(moves->size(), 2u);
    EXPECT_EQ((*moves)[0], (Transition{ 0u, true, Channel{ 0u, START } }));
    EXPECT_EQ((*moves)[1], (Transition{ 2u, false, Channel{ 0u, START } }));
  }

  TEST(Transitions, BlankLinesAndCommentsAreNotTransitions)
  {
    auto const moves{ TransitionsFrom(
      "# the intro\n\n  40 down p2.right  \n") };
    ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
    ASSERT_EQ(moves->size(), 1u);
    EXPECT_EQ((*moves)[0], (Transition{ 40u, true, Channel{ 1u, RIGHT } }));
  }

  TEST(Transitions, ABadLineIsRefusedByItsNumber)
  {
    auto const words{ TransitionsFrom(
      "0 down p1.start\n2 sideways p1.start\n") };
    ASSERT_FALSE(words.has_value());
    EXPECT_NE(words.error().find("line 2"), std::string::npos)
      << words.error();

    auto const channel{ TransitionsFrom("\n\n0 down p3.start\n") };
    ASSERT_FALSE(channel.has_value());
    EXPECT_NE(channel.error().find("line 3"), std::string::npos)
      << channel.error();

    auto const button{ TransitionsFrom("0 down p1.turbo\n") };
    ASSERT_FALSE(button.has_value());
    EXPECT_NE(button.error().find("turbo"), std::string::npos)
      << button.error();

    auto const shape{ TransitionsFrom("0 down\n") };
    ASSERT_FALSE(shape.has_value());
    EXPECT_NE(shape.error().find("line 1"), std::string::npos) << shape.error();
  }

  TEST(Transitions, TimeNeverGoesBackwards)
  {
    auto const moves{ TransitionsFrom("10 down p1.a\n4 up p1.a\n") };
    ASSERT_FALSE(moves.has_value());
    EXPECT_NE(moves.error().find("line 2"), std::string::npos);
  }

  TEST(Transitions, WhatIsWrittenReadsBackTheSame)
  {
    std::vector<Transition> const moves{
      Transition{ 0u, true, Channel{ 0u, START } },
      Transition{ 2u, false, Channel{ 0u, START } },
      Transition{ 120u, true, Channel{ 1u, RIGHT } }
    };
    auto const read{ TransitionsFrom(TextOf(moves)) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(*read, moves);
  }

  TEST(Transitions, EveryPadButtonHasAName)
  {
    for (unsigned button{ 0 }; button < BUTTON_NAMES.size(); ++button)
    {
      Channel const channel{ 1u, button };
      auto const read{ ChannelFrom(NameOf(channel)) };
      ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
      EXPECT_EQ(*read, channel);
      EXPECT_EQ(read->Mask(), std::uint32_t{ 1 } << button);
    }
  }
}
