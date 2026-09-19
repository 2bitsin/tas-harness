#include "tash/tape/tape.hpp"

#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <string>

namespace tash::tape
{
  namespace
  {
    [[nodiscard]] auto Sample() -> Tape
    {
      TapeHeader header{ };
      header.name           = "title-to-game";
      header.profile        = "examples/columns/profile.yaml";
      header.core           = "genesis_plus_gx";
      header.timeout_frames = 300u;
      header.on_timeout     = Recovery::RETRY;

      Segment first{ };
      first.name        = "skip-intro";
      first.anchor      = Anchor{ };
      first.anchor->kind = AnchorKind::EXACT_HASH;
      first.anchor->hash = "d90a8b4b09bd710f";
      first.frames      = 40u;
      first.transitions = "0      down p1.start\n2      up   p1.start\n";

      Segment second{ };
      second.name         = "walk-right";
      second.anchor       = Anchor{ };
      second.anchor->kind = AnchorKind::PERCEPTUAL_HASH;
      second.anchor->hash = "4ccc333399cc6633";
      second.anchor->max_distance = 6u;
      second.anchor->region = AnchorRegion{ 0u, 0u, 320u, 40u };
      second.transitions  = "0      down p1.right\n30     up   p1.right\n";

      return Tape{ header, { first, second } };
    }
  }

  TEST(TapeFile, WhatIsWrittenReadsBackTheSame)
  {
    auto const scratch{ utilities::ScratchAreaOf("tape-round-trip") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

    Tape const written{ Sample() };
    std::filesystem::path const path{ scratch->File("title-to-game.yaml") };
    ASSERT_TRUE(WriteTape(written, path).has_value());

    auto const read{ TapeFrom(path) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(*read, written);
  }

  TEST(TapeFile, WhatIsNotWrittenStaysOutOfTheFile)
  {
    auto const scratch{ utilities::ScratchAreaOf("tape-defaults") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

    Tape written{ };
    written.header.name = "bare";
    written.segments.push_back(Segment{ });
    written.segments.front().name = "only";

    std::filesystem::path const path{ scratch->File("bare.yaml") };
    ASSERT_TRUE(WriteTape(written, path).has_value());

    auto const read{ TapeFrom(path) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(*read, written);
    EXPECT_EQ(read->header.Timeout(), DEFAULT_TIMEOUT_FRAMES);
    EXPECT_EQ(read->header.OnTimeout(), Recovery::FAIL);
    EXPECT_EQ(read->segments.front().Waits().kind, AnchorKind::NONE);
  }

  TEST(TapeFile, ASegmentThatWillNotPlayIsRefusedByName)
  {
    Tape twice{ Sample() };
    twice.segments[1].name = twice.segments[0].name;
    auto const named{ Checked(twice) };
    ASSERT_FALSE(named.has_value());
    EXPECT_NE(named.error().find("skip-intro"), std::string::npos)
      << named.error();

    Tape wordless{ Sample() };
    wordless.segments[1].transitions = "0 down p1.start\n1 sideways p1.a\n";
    auto const lines{ Checked(wordless) };
    ASSERT_FALSE(lines.has_value());
    EXPECT_NE(lines.error().find("line 2"), std::string::npos)
      << lines.error();
    EXPECT_NE(lines.error().find("walk-right"), std::string::npos)
      << lines.error();

    Tape hashless{ Sample() };
    hashless.segments[0].anchor->hash = "not-a-hash";
    auto const hash{ Checked(hashless) };
    ASSERT_FALSE(hash.has_value());
    EXPECT_NE(hash.error().find("skip-intro"), std::string::npos)
      << hash.error();
  }

  TEST(TapeFile, TheExampleTapeIsOneThePlayerWouldTake)
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();

    auto const read{ TapeFrom(here / "examples/homebrew/tapes/demo.yaml") };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(read->segments.size(), 2u);
    EXPECT_EQ(read->header.Base(), TimeBase::FRAME);
  }
}
