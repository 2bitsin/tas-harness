#include "tash/linked/tash.hpp"

#include "_synthetic-target.hpp"

#include "tash/perception/frame-hash.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/tape/pointer.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tape/transitions.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tash::linked
{
  namespace
  {
    using detail::synthetic_target::FRAMES;
    using detail::synthetic_target::Guest;
    using detail::synthetic_target::HEIGHT;
    using detail::synthetic_target::Picture;
    using detail::synthetic_target::Play;
    using detail::synthetic_target::Sampled;
    using detail::synthetic_target::WIDTH;

    constexpr std::string_view TARGET{ "synthetic" };
    constexpr std::string_view PROFILE{ "examples/synthetic/profile.yaml" };

    struct Recorded
    {
      std::filesystem::path root{ };
      std::uint64_t         hash{ 0 };
    };

    // The whole run, into a directory of its own, and the digest the harness
    // computes for the picture it submitted.
    [[nodiscard]] auto Made(std::filesystem::path const& root) -> Recorded
    {
      std::vector<std::uint16_t> const picture{ Picture() };
      Guest guest{ };
      {
        Harness harness{ Options{ std::string{ TARGET },
                                  std::string{ PROFILE },
                                  Sink::DIRECTORY, root, 60.0 } };
        EXPECT_EQ(harness.Mode(), Mode::FILE);
        Play(harness, guest, picture);
      }
      auto const hashed{ perception::ExactHash(perception::ViewOf(
        picture.data(), WIDTH, HEIGHT, WIDTH * 2u)) };
      EXPECT_TRUE(hashed.has_value());
      return Recorded{ root, hashed.value_or(0u) };
    }

    // The device state a tape read back says the target held on each frame.
    [[nodiscard]] auto Replayed(tape::Tape const& read)
      -> std::vector<Input>
    {
      std::vector<Input> states(FRAMES, Input{ });
      tape::Segment const& segment{ read.segments.front() };
      auto const moves{ tape::TransitionsFrom(segment.Lines()) };
      auto const points{ tape::PointerMovesFrom(segment.Points()) };
      EXPECT_TRUE(moves.has_value());
      EXPECT_TRUE(points.has_value());

      Input held{ };
      for (std::uint64_t frame{ 0 }; frame < FRAMES; ++frame)
      {
        for (tape::Transition const& move : moves.value_or(
               std::vector<tape::Transition>{ }))
        {
          if (move.frame != frame)
            continue;
          std::uint32_t& bits{
            move.channel.device == tape::Device::MOUSE
              ? held.mice[move.channel.port].buttons
              : held.pads[move.channel.port].buttons };
          bits = move.down ? bits | move.channel.Mask()
                           : bits & ~move.channel.Mask();
        }
        for (tape::PointerMove const& point : points.value_or(
               std::vector<tape::PointerMove>{ }))
        {
          if (point.frame != frame)
            continue;
          held.mice[point.port].x = point.x;
          held.mice[point.port].y = point.y;
        }
        states[frame] = held;
      }
      return states;
    }

    // A tape holds no axes and no keys, so what it can be equal in is the
    // buttons and where the pointer was.
    [[nodiscard]] auto Same(Input const& left, Input const& right) -> bool
    {
      for (std::size_t port{ 0 }; port < PADS; ++port)
        if (left.pads[port].buttons != right.pads[port].buttons)
          return false;
      for (std::size_t port{ 0 }; port < MICE; ++port)
        if (left.mice[port] != right.mice[port])
          return false;
      return true;
    }
  }

  TEST(FileMode, TheTapeIsOneEveryReaderTakes)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-tape") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    Recorded const made{ Made(scratch->File("run")) };

    auto const read{ tape::TapeFrom(made.root / "tape.yaml") };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(read->header.name, TARGET);
    ASSERT_EQ(read->segments.size(), 1u);
    EXPECT_EQ(read->segments.front().name, tape::FIRST_SEGMENT_NAME);
    EXPECT_EQ(read->segments.front().frames, FRAMES);
  }

  TEST(FileMode, WhatTheTargetSampledIsWhatTheTapeSaysItHeld)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-replay") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    Recorded const made{ Made(scratch->File("run")) };

    auto const read{ tape::TapeFrom(made.root / "tape.yaml") };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());

    std::vector<Input> const states{ Replayed(*read) };
    ASSERT_EQ(states.size(), FRAMES);
    for (std::uint64_t frame{ 0 }; frame < FRAMES; ++frame)
      EXPECT_TRUE(Same(states[frame], Sampled(frame)))
        << "frame " << frame;
  }

  TEST(FileMode, TheTraceHoldsTheFramesTheWatchesAndTheEvent)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-trace") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    Recorded const made{ Made(scratch->File("run")) };

    auto reader{ trace::Reader::Open(made.root / "trace.bin") };
    ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());

    std::vector<trace::FrameRecord>   frames;
    std::map<std::uint32_t, std::int64_t> last;
    std::vector<trace::TriggerRecord> events;
    std::vector<trace::DecisionRecord> decisions;
    reader->ForEach([&](auto const& held)
    {
      using Held = std::decay_t<decltype(held)>;
      if constexpr (std::is_same_v<Held, trace::FrameRecord>)
        frames.push_back(held);
      else if constexpr (std::is_same_v<Held, trace::WatchRecord>)
        last[held.watch] = held.value;
      else if constexpr (std::is_same_v<Held, trace::TriggerRecord>)
        events.push_back(held);
      else if constexpr (std::is_same_v<Held, trace::DecisionRecord>)
        decisions.push_back(held);
    });
    EXPECT_FALSE(reader->Truncated());
    EXPECT_EQ(reader->Unknown(), 0u);

    ASSERT_EQ(frames.size(), FRAMES);
    for (std::uint64_t frame{ 0 }; frame < FRAMES; ++frame)
    {
      EXPECT_EQ(frames[frame].frame, frame);
      EXPECT_EQ(frames[frame].hash_exact, made.hash);
    }

    ASSERT_EQ(last.size(), 3u);
    EXPECT_EQ(last[0], static_cast<std::int64_t>(3u + FRAMES - 1u));
    EXPECT_EQ(last[1], -7 + static_cast<std::int64_t>(FRAMES - 1u));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().frame, detail::synthetic_target::EVENT_FRAME);
    EXPECT_EQ(events.front().name, detail::synthetic_target::EVENT_NAME);
    EXPECT_EQ(events.front().text, detail::synthetic_target::EVENT_TEXT);

    ASSERT_EQ(decisions.size(), 2u);
    EXPECT_NE(decisions[0].text.find(
      std::to_string(detail::synthetic_target::SEED)), std::string::npos);
  }

  TEST(FileMode, TheDirectoryIsABundleARunManifestOpens)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-bundle") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    Recorded const made{ Made(scratch->File("run")) };

    auto const bundle{ recorder::Bundle::At(made.root) };
    ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
    auto const manifest{ bundle->Read() };
    ASSERT_TRUE(manifest.has_value()) << (manifest ? "" : manifest.error());

    EXPECT_EQ(manifest->core_name, TARGET);
    EXPECT_EQ(manifest->profile, PROFILE);
    EXPECT_EQ(manifest->frames, FRAMES);
    EXPECT_EQ(manifest->tape, "tape.yaml");
    ASSERT_TRUE(manifest->watches.has_value());
    EXPECT_EQ(*manifest->watches,
              (std::vector<std::string>{ "room", "score", "step" }));
  }
}
