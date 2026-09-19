#include "tash/tape/recorder.hpp"

#include "tash/tape/transitions.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace tash::tape
{
  namespace
  {
    constexpr std::uint32_t START{ 1u << 3 };
    constexpr std::uint32_t RIGHT{ 1u << 7 };

    [[nodiscard]] auto Header() -> TapeHeader
    {
      TapeHeader header{ };
      header.name = "recorded";
      return header;
    }

    [[nodiscard]] auto MovesOf(Segment const& segment)
      -> std::vector<Transition>
    {
      auto const moves{ TransitionsFrom(segment.Lines()) };
      EXPECT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
      return moves.value_or(std::vector<Transition>{ });
    }
  }

  TEST(TapeRecorder, TheBitsThatMovedBecomeTransitions)
  {
    Recorder recording{ Header() };
    recording.Change(0u, 0u, START);
    recording.Change(2u, 0u, 0u);
    recording.Change(40u, 0u, RIGHT | START);
    Tape const written{ recording.Finish(60u) };

    ASSERT_EQ(written.segments.size(), 1u);
    EXPECT_EQ(written.segments[0].name, FIRST_SEGMENT_NAME);
    EXPECT_EQ(written.segments[0].frames, 60u);

    std::vector<Transition> const moves{ MovesOf(written.segments[0]) };
    ASSERT_EQ(moves.size(), 4u);
    EXPECT_EQ(NameOf(moves[0].channel), "p1.start");
    EXPECT_TRUE(moves[0].down);
    EXPECT_EQ(moves[1].frame, 2u);
    EXPECT_FALSE(moves[1].down);
    EXPECT_EQ(moves[2].frame, 40u);
    EXPECT_EQ(NameOf(moves[2].channel), "p1.start");
    EXPECT_EQ(NameOf(moves[3].channel), "p1.right");
    EXPECT_EQ(recording.Transitions(), 4u);
  }

  TEST(TapeRecorder, AMarkCutsTheSegmentAndFramesStartAgain)
  {
    Anchor waited{ };
    waited.kind = AnchorKind::EXACT_HASH;
    waited.hash = "d90a8b4b09bd710f";

    Recorder recording{ Header() };
    recording.Change(10u, 0u, START);
    recording.Mark(30u, "in-game", waited);
    recording.Change(35u, 1u, RIGHT);
    Tape const written{ recording.Finish(50u) };

    ASSERT_EQ(written.segments.size(), 2u);
    EXPECT_EQ(written.segments[0].frames, 30u);
    EXPECT_EQ(written.segments[1].name, "in-game");
    EXPECT_EQ(written.segments[1].Waits(), waited);
    EXPECT_EQ(written.segments[1].frames, 20u);

    std::vector<Transition> const moves{ MovesOf(written.segments[1]) };
    ASSERT_EQ(moves.size(), 1u);
    EXPECT_EQ(moves[0].frame, 5u);
    EXPECT_EQ(NameOf(moves[0].channel), "p2.right");
  }

  TEST(TapeRecorder, AMarkOnAnEmptySegmentNamesItInstead)
  {
    Recorder recording{ Header() };
    recording.Mark(0u, "title", Anchor{ });
    recording.Change(4u, 0u, START);
    Tape const written{ recording.Finish(20u) };

    ASSERT_EQ(written.segments.size(), 1u);
    EXPECT_EQ(written.segments[0].name, "title");
  }

  TEST(TapeRecorder, ARecordedTapeIsOneTheModuleWouldRead)
  {
    Recorder recording{ Header() };
    recording.Change(0u, 0u, START);
    recording.Change(3u, 0u, 0u);
    recording.Mark(30u, "in-game", Anchor{ });
    recording.Change(31u, 0u, RIGHT);
    recording.Change(61u, 0u, 0u);
    Tape const written{ recording.Finish(90u) };

    EXPECT_TRUE(Checked(written).has_value());
  }
}
