#include "tash/tape/pointer.hpp"

#include "tash/tape/channel.hpp"
#include "tash/tape/transitions.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace tash::tape
{
  TEST(Pointer, ALineIsAFrameADeviceAndAPoint)
  {
    auto const moves{ PointerMovesFrom("0 m1 128,96\n3 m1 -4,200\n") };
    ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
    ASSERT_EQ(moves->size(), 2u);
    EXPECT_EQ((*moves)[0], (PointerMove{ 0u, 0u, 128, 96 }));
    EXPECT_EQ((*moves)[1], (PointerMove{ 3u, 0u, -4, 200 }));
  }

  TEST(Pointer, BlankLinesAndCommentsAreNotMoves)
  {
    auto const moves{ PointerMovesFrom("# where it starts\n\n  9 m2 1,2  \n") };
    ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
    ASSERT_EQ(moves->size(), 1u);
    EXPECT_EQ((*moves)[0], (PointerMove{ 9u, 1u, 1, 2 }));
  }

  TEST(Pointer, APadIsNotAPointer)
  {
    auto const moves{ PointerMovesFrom("0 p1 4,4\n") };
    ASSERT_FALSE(moves.has_value());
    EXPECT_NE(moves.error().find("line 1"), std::string::npos);
  }

  TEST(Pointer, APointIsTwoNumbers)
  {
    auto const moves{ PointerMovesFrom("0 m1 128\n") };
    ASSERT_FALSE(moves.has_value());
    EXPECT_NE(moves.error().find("<x>,<y>"), std::string::npos);
  }

  TEST(Pointer, FramesDoNotGoBackwards)
  {
    auto const moves{ PointerMovesFrom("10 m1 1,1\n4 m1 2,2\n") };
    ASSERT_FALSE(moves.has_value());
    EXPECT_NE(moves.error().find("line 2"), std::string::npos);
  }

  TEST(Pointer, WhatIsWrittenReadsBackTheSame)
  {
    std::vector<PointerMove> const moves{
      PointerMove{ 0u, 0u, 128, 96 },
      PointerMove{ 1u, 0u, 130, 96 },
      PointerMove{ 40u, 1u, -1, 0 }
    };
    auto const read{ PointerMovesFrom(TextOf(moves)) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(*read, moves);
  }

  TEST(Pointer, EveryMouseButtonHasAChannelName)
  {
    for (unsigned button{ 0 }; button < MOUSE_BUTTON_NAMES.size(); ++button)
    {
      Channel const channel{ 0u, button, Device::MOUSE };
      auto const read{ ChannelFrom(NameOf(channel)) };
      ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
      EXPECT_EQ(*read, channel);
      EXPECT_EQ(read->Mask(), std::uint32_t{ 1 } << button);
    }
  }

  TEST(Pointer, AMouseButtonIsATransitionLikeAPadButton)
  {
    auto const moves{ TransitionsFrom("6 down m1.left\n9 up m1.left\n") };
    ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
    ASSERT_EQ(moves->size(), 2u);
    EXPECT_EQ((*moves)[0].channel.device, Device::MOUSE);
    EXPECT_EQ((*moves)[0].channel.port, 0u);
  }

  TEST(Pointer, APadHasNoLeftButton)
  {
    auto const moves{ TransitionsFrom("0 down p1.left\n0 down p1.wheel_up\n") };
    ASSERT_FALSE(moves.has_value());
    EXPECT_NE(moves.error().find("line 2"), std::string::npos);
  }
}
